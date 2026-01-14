/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kVisor syscall interposition layer
 *
 * This header defines the interface between the C interposition layer
 * and the Rust kVisor implementation.
 */

#ifndef _KVISOR_INTERPOSE_H
#define _KVISOR_INTERPOSE_H

#include <linux/types.h>
#include <linux/sched.h>

struct pt_regs;

/*
 * Check if the current task is running under kVisor supervision.
 * Returns true if the task is sandboxed.
 */
bool kvisor_is_sandboxed(struct task_struct *task);

/*
 * Mark a task as running under kVisor supervision.
 * Called during kvisor_exec().
 */
void kvisor_set_sandboxed(struct task_struct *task);

/*
 * Clear the kVisor sandbox flag from a task.
 * Called during task cleanup.
 */
void kvisor_clear_sandboxed(struct task_struct *task);

/*
 * Main syscall interposition entry point.
 * Called from do_syscall_64() for sandboxed tasks.
 *
 * Returns: true if kVisor handled the syscall (caller should not
 *          invoke normal syscall path), false otherwise.
 */
bool kvisor_syscall_interpose(struct pt_regs *regs, int nr);

/*
 * Initialize kVisor subsystem.
 * Called during kernel boot.
 */
int kvisor_init(void);

/*
 * Cleanup kVisor subsystem.
 * Called during kernel shutdown.
 */
void kvisor_exit(void);

/*
 * Per-task kVisor context management
 */
struct kvisor_task_ctx;

struct kvisor_task_ctx *kvisor_alloc_task_ctx(void);
void kvisor_free_task_ctx(struct kvisor_task_ctx *ctx);

/*
 * kvisor_exec - Execute a program in the kVisor sandbox
 * @path: Path to the executable
 * @argv: Argument vector (NULL-terminated)
 * @envp: Environment vector (NULL-terminated)
 *
 * This is the main entry point for spawning sandboxed processes.
 * Returns the child PID on success, negative error code on failure.
 */
long kvisor_exec(const char __user *path,
		 const char __user *const __user *argv,
		 const char __user *const __user *envp);

#endif /* _KVISOR_INTERPOSE_H */
