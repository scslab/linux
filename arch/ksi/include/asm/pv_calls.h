/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_PV_CALLS_H
#define __ASM_KSI_PV_CALLS_H

/*
 * KSI Hypercall API
 *
 * All hardware access is replaced with ksi_* hypercalls to the host
 * environment (microkernel, hypervisor, user-mode process, etc.).
 */

#include <linux/types.h>

struct ksi_boot_info {
	phys_addr_t	mem_start;
	phys_addr_t	mem_end;
	phys_addr_t	initrd_start;
	phys_addr_t	initrd_end;
	const char	*cmdline;
};

typedef unsigned long ksi_asid_t;

/* Boot */
void ksi_get_boot_info(struct ksi_boot_info *info);

/* Console */
void ksi_console_write(const char *buf, unsigned int len);
int ksi_console_read(char *buf, unsigned int len);

/* Address space management */
ksi_asid_t ksi_as_create(void);
void ksi_as_destroy(ksi_asid_t asid);
int ksi_as_map(ksi_asid_t asid, unsigned long vaddr, phys_addr_t paddr,
	       unsigned long size, unsigned long prot);
int ksi_as_unmap(ksi_asid_t asid, unsigned long vaddr, unsigned long size);

/* Kernel address space mapping (for linear map) */
int ksi_kern_map(unsigned long vaddr, phys_addr_t paddr, unsigned long size,
		 unsigned long prot);

/* Execution */
int ksi_user_resume(void);

/* Time */
u64 ksi_time_now(void);
void ksi_timer_arm(u64 deadline_ns);
void ksi_timer_disarm(void);

/* SMP */
int ksi_cpu_start(unsigned int cpu, unsigned long entry);
void ksi_send_ipi(unsigned int cpu);
void ksi_wait_for_event(void);

/* System */
void ksi_halt(void) __attribute__((noreturn));
void ksi_yield(void);

/* Early boot console — registers before start_kernel() */
void ksi_setup_early_console(void);

#endif /* __ASM_KSI_PV_CALLS_H */
