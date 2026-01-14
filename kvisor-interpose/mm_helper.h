/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kVisor memory mapping helpers
 */

#ifndef _KVISOR_MM_HELPER_H
#define _KVISOR_MM_HELPER_H

/*
 * Map memory in the current process.
 * Returns mapped address or negative error.
 */
unsigned long kvisor_mmap(unsigned long addr, unsigned long len,
			  unsigned long prot, unsigned long flags);

/*
 * Unmap memory in the current process.
 */
int kvisor_munmap(unsigned long addr, unsigned long len);

/*
 * Copy kernel data to userspace address.
 */
int kvisor_copy_to_user_addr(unsigned long to, const void *from, unsigned long len);

/*
 * Zero out userspace memory.
 */
int kvisor_clear_user_addr(unsigned long addr, unsigned long len);

/*
 * Clear address space for new executable.
 */
int kvisor_flush_old_exec(void);

/*
 * Change memory protection.
 */
int kvisor_mprotect(unsigned long addr, unsigned long len, unsigned long prot);

/* Protection flags (same as userspace PROT_*) */
#define KVISOR_PROT_READ	0x1
#define KVISOR_PROT_WRITE	0x2
#define KVISOR_PROT_EXEC	0x4

/* Map flags (same as userspace MAP_*) */
#define KVISOR_MAP_PRIVATE	0x02
#define KVISOR_MAP_FIXED	0x10
#define KVISOR_MAP_ANONYMOUS	0x20

#endif /* _KVISOR_MM_HELPER_H */
