// SPDX-License-Identifier: GPL-2.0
/*
 * kVisor syscall interposition layer
 *
 * This module provides the C-side glue between the Linux kernel's
 * syscall handling and the Rust kVisor implementation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/ptrace.h>
#include <linux/syscalls.h>
#include <asm/syscall.h>

#include "interpose.h"

/*
 * We use a bit in task_struct->flags or a custom field to track
 * whether a task is sandboxed. For now, we use a simple approach
 * with a hash table keyed by task pointer.
 *
 * TODO: Add a proper field to task_struct for production use.
 */

#include <linux/hashtable.h>
#include <linux/spinlock.h>

#define KVISOR_TASK_HASH_BITS 10

struct kvisor_task_entry {
	struct task_struct *task;
	struct kvisor_task_ctx *ctx;
	struct hlist_node node;
};

static DEFINE_HASHTABLE(kvisor_tasks, KVISOR_TASK_HASH_BITS);
static DEFINE_SPINLOCK(kvisor_tasks_lock);

/* Forward declaration of Rust entry point */
extern long rust_kvisor_handle_syscall(struct pt_regs *regs, int nr,
				       struct kvisor_task_ctx *ctx);
extern struct kvisor_task_ctx *rust_kvisor_alloc_ctx(void);
extern void rust_kvisor_free_ctx(struct kvisor_task_ctx *ctx);
extern int rust_kvisor_init(void);
extern void rust_kvisor_exit(void);

static struct kvisor_task_entry *find_task_entry(struct task_struct *task)
{
	struct kvisor_task_entry *entry;
	unsigned long key = (unsigned long)task;

	hash_for_each_possible(kvisor_tasks, entry, node, key) {
		if (entry->task == task)
			return entry;
	}
	return NULL;
}

bool kvisor_is_sandboxed(struct task_struct *task)
{
	struct kvisor_task_entry *entry;
	bool sandboxed;
	unsigned long flags;

	spin_lock_irqsave(&kvisor_tasks_lock, flags);
	entry = find_task_entry(task);
	sandboxed = (entry != NULL);
	spin_unlock_irqrestore(&kvisor_tasks_lock, flags);

	return sandboxed;
}
EXPORT_SYMBOL_GPL(kvisor_is_sandboxed);

void kvisor_set_sandboxed(struct task_struct *task)
{
	struct kvisor_task_entry *entry;
	unsigned long flags;
	unsigned long key = (unsigned long)task;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		pr_err("kvisor: failed to allocate task entry\n");
		return;
	}

	entry->task = task;
	entry->ctx = rust_kvisor_alloc_ctx();
	if (!entry->ctx) {
		kfree(entry);
		pr_err("kvisor: failed to allocate task context\n");
		return;
	}

	spin_lock_irqsave(&kvisor_tasks_lock, flags);
	hash_add(kvisor_tasks, &entry->node, key);
	spin_unlock_irqrestore(&kvisor_tasks_lock, flags);

	pr_debug("kvisor: task %d marked as sandboxed\n", task->pid);
}
EXPORT_SYMBOL_GPL(kvisor_set_sandboxed);

void kvisor_clear_sandboxed(struct task_struct *task)
{
	struct kvisor_task_entry *entry;
	unsigned long flags;

	spin_lock_irqsave(&kvisor_tasks_lock, flags);
	entry = find_task_entry(task);
	if (entry) {
		hash_del(&entry->node);
		spin_unlock_irqrestore(&kvisor_tasks_lock, flags);

		rust_kvisor_free_ctx(entry->ctx);
		kfree(entry);
		pr_debug("kvisor: task %d removed from sandbox\n", task->pid);
	} else {
		spin_unlock_irqrestore(&kvisor_tasks_lock, flags);
	}
}
EXPORT_SYMBOL_GPL(kvisor_clear_sandboxed);

bool kvisor_syscall_interpose(struct pt_regs *regs, int nr)
{
	struct kvisor_task_entry *entry;
	unsigned long flags;
	long ret;

	spin_lock_irqsave(&kvisor_tasks_lock, flags);
	entry = find_task_entry(current);
	spin_unlock_irqrestore(&kvisor_tasks_lock, flags);

	if (!entry)
		return false;

	/* Hand off to Rust implementation */
	ret = rust_kvisor_handle_syscall(regs, nr, entry->ctx);
	regs->ax = ret;

	return true;
}
EXPORT_SYMBOL_GPL(kvisor_syscall_interpose);

struct kvisor_task_ctx *kvisor_alloc_task_ctx(void)
{
	return rust_kvisor_alloc_ctx();
}
EXPORT_SYMBOL_GPL(kvisor_alloc_task_ctx);

void kvisor_free_task_ctx(struct kvisor_task_ctx *ctx)
{
	rust_kvisor_free_ctx(ctx);
}
EXPORT_SYMBOL_GPL(kvisor_free_task_ctx);

/*
 * kvisor_exec syscall implementation
 *
 * This creates a new process that runs under kVisor supervision.
 * The parent process remains in normal Linux mode.
 */
long kvisor_exec(const char __user *path,
		 const char __user *const __user *argv,
		 const char __user *const __user *envp)
{
	/*
	 * TODO: Implement process spawning
	 *
	 * 1. Fork a new process
	 * 2. Mark child as sandboxed
	 * 3. Set up kVisor context (fd table, memory map, etc.)
	 * 4. Exec the target binary in the child
	 * 5. Return child PID to parent
	 */
	pr_info("kvisor: kvisor_exec called (not yet implemented)\n");
	return -ENOSYS;
}
EXPORT_SYMBOL_GPL(kvisor_exec);

int kvisor_init(void)
{
	int ret;

	pr_info("kvisor: initializing\n");

	ret = rust_kvisor_init();
	if (ret) {
		pr_err("kvisor: Rust initialization failed: %d\n", ret);
		return ret;
	}

	pr_info("kvisor: initialized successfully\n");
	return 0;
}

void kvisor_exit(void)
{
	struct kvisor_task_entry *entry;
	struct hlist_node *tmp;
	unsigned long flags;
	int bkt;

	pr_info("kvisor: shutting down\n");

	/* Clean up any remaining sandboxed tasks */
	spin_lock_irqsave(&kvisor_tasks_lock, flags);
	hash_for_each_safe(kvisor_tasks, bkt, tmp, entry, node) {
		hash_del(&entry->node);
		rust_kvisor_free_ctx(entry->ctx);
		kfree(entry);
	}
	spin_unlock_irqrestore(&kvisor_tasks_lock, flags);

	rust_kvisor_exit();

	pr_info("kvisor: shutdown complete\n");
}

/* Register kvisor_exec as a syscall */
#ifdef CONFIG_KVISOR
static int __init kvisor_module_init(void)
{
	return kvisor_init();
}

static void __exit kvisor_module_exit(void)
{
	kvisor_exit();
}

module_init(kvisor_module_init);
module_exit(kvisor_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kVisor Authors");
MODULE_DESCRIPTION("kVisor kernel-based sandbox");
#endif
