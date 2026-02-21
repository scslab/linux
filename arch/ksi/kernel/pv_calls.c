// SPDX-License-Identifier: GPL-2.0-only
/*
 * KSI paravirtualization call stubs
 *
 * All ksi_*() functions panic("not implemented") for the skeleton.
 * Each will be filled in with the actual host interface implementation.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/start_kernel.h>
#include <asm/memory.h>
#include <asm/sysreg.h>
#include <asm/pv_calls.h>

/* From arm64 mm/mmu.c */
extern u64 kimage_voffset;

/* Forward declaration */
asmlinkage void ksi_start_kernel(void);

/* Entry point called from head.S */
asmlinkage void __init ksi_start_kernel(void)
{
	extern struct task_struct init_task;

	/*
	 * Initialize current task pointer in SP_EL0.
	 * arm64 normally does this in head.S.
	 */
	write_sysreg(&init_task, sp_el0);

	/*
	 * Initialize kimage_voffset for VA/PA translation.
	 *
	 * kimage_voffset: offset from kernel VA to PA (KIMAGE_VADDR - phys).
	 * The host loads the kernel at physical address 0 and maps it
	 * at KIMAGE_VADDR.  memstart_addr is set later by arm64_memblock_init().
	 */
	kimage_voffset = KIMAGE_VADDR;

	/* Register the KSI early console so printk output is visible */
	ksi_setup_early_console();

	start_kernel();
}

/* Boot */
void ksi_get_boot_info(struct ksi_boot_info *info)
{
	panic("ksi_get_boot_info: not implemented");
}

/* Console */
void ksi_console_write(const char *buf, unsigned int len)
{
	panic("ksi_console_write: not implemented");
}

int ksi_console_read(char *buf, unsigned int len)
{
	panic("ksi_console_read: not implemented");
}

/* Address space management */
ksi_asid_t ksi_as_create(void)
{
	panic("ksi_as_create: not implemented");
}

void ksi_as_destroy(ksi_asid_t asid)
{
	panic("ksi_as_destroy: not implemented");
}

int ksi_as_map(ksi_asid_t asid, unsigned long vaddr, phys_addr_t paddr,
	       unsigned long size, unsigned long prot)
{
	panic("ksi_as_map: not implemented");
}

int ksi_as_unmap(ksi_asid_t asid, unsigned long vaddr, unsigned long size)
{
	panic("ksi_as_unmap: not implemented");
}

int ksi_kern_map(unsigned long vaddr, phys_addr_t paddr, unsigned long size,
		 unsigned long prot)
{
	panic("ksi_kern_map: not implemented");
}

/* Execution */
int ksi_user_resume(void)
{
	panic("ksi_user_resume: not implemented");
}

/* Time */
u64 ksi_time_now(void)
{
	panic("ksi_time_now: not implemented");
}

void ksi_timer_arm(u64 deadline_ns)
{
	panic("ksi_timer_arm: not implemented");
}

void ksi_timer_disarm(void)
{
	panic("ksi_timer_disarm: not implemented");
}

/* SMP */
int ksi_cpu_start(unsigned int cpu, unsigned long entry)
{
	panic("ksi_cpu_start: not implemented");
}

void ksi_send_ipi(unsigned int cpu)
{
	panic("ksi_send_ipi: not implemented");
}

void ksi_wait_for_event(void)
{
	panic("ksi_wait_for_event: not implemented");
}

/* System */
void ksi_halt(void)
{
	panic("ksi_halt: not implemented");
}

void ksi_yield(void)
{
	panic("ksi_yield: not implemented");
}
