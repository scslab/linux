// SPDX-License-Identifier: GPL-2.0
/*
 * kVisor memory mapping helpers
 *
 * These functions provide memory mapping capabilities for setting up
 * sandboxed process address spaces.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched/mm.h>
#include <linux/highmem.h>
#include <linux/uaccess.h>
#include <asm/tlbflush.h>

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
 * kvisor_flush_old_exec - Prepare address space for new executable
 *
 * This clears the current process's address space, similar to what
 * happens during execve(). Should be called before loading a new binary.
 *
 * Returns 0 on success, negative error on failure.
 */
int kvisor_flush_old_exec(void)
{
	struct mm_struct *mm = current->mm;
	int ret;

	if (!mm)
		return -EINVAL;

	pr_info("kvisor: Flushing old address space for pid %d\n", current->pid);

	/*
	 * Unmap the entire user address space.
	 * This is a simplified version - production code would be more careful
	 * about things like vdso, etc.
	 */
	ret = vm_munmap(0, TASK_SIZE);
	if (ret < 0) {
		pr_err("kvisor: vm_munmap failed: %d\n", ret);
		return ret;
	}

	pr_info("kvisor: Address space flushed successfully\n");
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
