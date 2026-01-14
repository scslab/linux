// SPDX-License-Identifier: GPL-2.0
/*
 * kVisor memory mapping helpers
 *
 * These functions provide memory mapping capabilities for setting up
 * sandboxed process address spaces.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/highmem.h>
#include <linux/uaccess.h>
#include <linux/rwsem.h>
#include <asm/tlbflush.h>
#include <asm/mmu_context.h>

#include "mm_helper.h"

/*
 * kvisor_mmap - Map memory in the current process
 * @addr: Desired virtual address (hint, may be ignored if not MAP_FIXED)
 * @len: Length of mapping
 * @prot: Protection flags (PROT_READ, PROT_WRITE, PROT_EXEC)
 * @flags: Mapping flags (MAP_PRIVATE, MAP_ANONYMOUS, MAP_FIXED)
 *
 * Returns the actual mapped address, or a negative error code.
 */
unsigned long kvisor_mmap(unsigned long addr, unsigned long len,
			  unsigned long prot, unsigned long flags)
{
	return vm_mmap(NULL, addr, len, prot, flags, 0);
}
EXPORT_SYMBOL_GPL(kvisor_mmap);

/*
 * kvisor_munmap - Unmap memory in the current process
 * @addr: Start address of region to unmap
 * @len: Length of region to unmap
 *
 * Returns 0 on success, negative error code on failure.
 */
int kvisor_munmap(unsigned long addr, unsigned long len)
{
	return vm_munmap(addr, len);
}
EXPORT_SYMBOL_GPL(kvisor_munmap);

/*
 * kvisor_copy_to_user_addr - Copy kernel data to a userspace address
 * @to: Userspace destination address
 * @from: Kernel source buffer
 * @len: Number of bytes to copy
 *
 * This is a wrapper around copy_to_user for use from Rust.
 * Returns 0 on success, -EFAULT on failure.
 */
int kvisor_copy_to_user_addr(unsigned long to, const void *from, unsigned long len)
{
	if (copy_to_user((void __user *)to, from, len))
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_GPL(kvisor_copy_to_user_addr);

/*
 * kvisor_clear_user_addr - Zero out a userspace memory region
 * @addr: Userspace address to clear
 * @len: Number of bytes to clear
 *
 * Returns 0 on success, -EFAULT on failure.
 */
int kvisor_clear_user_addr(unsigned long addr, unsigned long len)
{
	if (clear_user((void __user *)addr, len))
		return -EFAULT;
	return 0;
}
EXPORT_SYMBOL_GPL(kvisor_clear_user_addr);

/*
 * kvisor_flush_old_exec - Flush the parent's address space
 *
 * This is modeled after Linux's exec_mmap() from fs/exec.c.
 * It creates a new mm_struct for the current process, discarding
 * all inherited mappings from the parent (after fork). This is
 * essential before loading a new executable to:
 *   1. Clear stale TLB entries
 *   2. Release parent's memory mappings (COW pages)
 *   3. Start with a clean address space
 *   4. Properly synchronize with other kernel subsystems
 *
 * Returns 0 on success, negative error code on failure.
 */
int kvisor_flush_old_exec(void)
{
	struct task_struct *tsk = current;
	struct mm_struct *mm, *old_mm, *active_mm;
	int ret;

	old_mm = tsk->mm;
	if (!old_mm) {
		pr_err("kvisor: flush_old_exec called with no mm\n");
		return -EINVAL;
	}

	/* Allocate a new mm_struct */
	mm = mm_alloc();
	if (!mm)
		return -ENOMEM;

	/* Initialize the new address space (arch-specific context) */
	ret = init_new_context(tsk, mm);
	if (ret) {
		mmdrop(mm);
		return ret;
	}

	/*
	 * Notify parent that we're no longer interested in the old VM.
	 * This handles vfork() completion and futex cleanup.
	 */
	exec_mm_release(tsk, old_mm);

	/*
	 * Take the exec_update_lock to serialize with other operations
	 * that read the mm (like /proc, ptrace, etc.)
	 */
	ret = down_write_killable(&tsk->signal->exec_update_lock);
	if (ret) {
		mmdrop(mm);
		return ret;
	}

	/*
	 * Take mmap_read_lock on old_mm. This is killable so we can
	 * respond to fatal signals during exec.
	 */
	ret = mmap_read_lock_killable(old_mm);
	if (ret) {
		up_write(&tsk->signal->exec_update_lock);
		mmdrop(mm);
		return ret;
	}

	/*
	 * Now perform the actual mm switch under task_lock with
	 * interrupts disabled to prevent races with context switches
	 * and lazy TLB handling.
	 */
	task_lock(tsk);

	/* Memory barrier for membarrier syscall users */
	membarrier_exec_mmap(mm);

	/*
	 * Disable interrupts during the switch. This prevents preemption
	 * while active_mm is being updated, which could cause problems
	 * for lazy TLB mm refcounting.
	 */
	local_irq_disable();

	active_mm = tsk->active_mm;
	tsk->active_mm = mm;
	tsk->mm = mm;

	/* Initialize per-CPU mm state */
	mm_init_cid(mm, tsk);

	/*
	 * Switch the MMU context. This flushes TLB entries for the old mm
	 * and loads the page table for the new mm.
	 */
	if (!IS_ENABLED(CONFIG_ARCH_WANT_IRQS_OFF_ACTIVATE_MM))
		local_irq_enable();
	activate_mm(active_mm, mm);
	if (IS_ENABLED(CONFIG_ARCH_WANT_IRQS_OFF_ACTIVATE_MM))
		local_irq_enable();

	/* Add to LRU generation tracking for memory reclaim */
	lru_gen_add_mm(mm);

	task_unlock(tsk);

	/* Mark the mm as in use for LRU tracking */
	lru_gen_use_mm(mm);

	/*
	 * Release locks and clean up old_mm.
	 */
	mmap_read_unlock(old_mm);

	/* Sanity check: active_mm should have been old_mm */
	BUG_ON(active_mm != old_mm);

	/* Update RSS high-water mark for accounting */
	setmax_mm_hiwater_rss(&tsk->signal->maxrss, old_mm);

	/* Update mm ownership for OOM killer */
	mm_update_next_owner(old_mm);

	/* Release the old mm (decrements refcount, may free it) */
	mmput(old_mm);

	/* Release exec_update_lock */
	up_write(&tsk->signal->exec_update_lock);

	pr_info("kvisor: flushed old address space for pid %d\n", tsk->pid);
	return 0;
}
EXPORT_SYMBOL_GPL(kvisor_flush_old_exec);

/*
 * kvisor_mprotect - Change protection on a memory region
 * @addr: Start address of region
 * @len: Length of region
 * @prot: New protection flags
 *
 * Note: For Phase 1, this is a no-op. We map segments with final permissions.
 * A full implementation would need to use mprotect_fixup or similar internal APIs.
 *
 * Returns 0 on success, negative error on failure.
 */
int kvisor_mprotect(unsigned long addr, unsigned long len, unsigned long prot)
{
	/* TODO: Implement proper mprotect support */
	(void)addr;
	(void)len;
	(void)prot;
	return 0;
}
EXPORT_SYMBOL_GPL(kvisor_mprotect);
