// SPDX-License-Identifier: GPL-2.0
/*
 * kVisor entry point helpers
 *
 * These functions help set up the CPU state to jump to a new entry point
 * when returning to userspace.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>
#include <asm/ptrace.h>
#include <asm/processor.h>

#include "entry_helper.h"

/*
 * kvisor_setup_entry - Set up registers to jump to entry point on syscall return
 * @entry: Entry point address (RIP)
 * @sp: Stack pointer (RSP)
 *
 * This modifies the current task's saved registers so that when the
 * kernel returns from the current syscall, it will jump to the specified
 * entry point with the given stack pointer.
 *
 * All general purpose registers are cleared except RSP.
 *
 * Returns 0 on success, negative error code on failure.
 */
int kvisor_setup_entry(unsigned long entry, unsigned long sp)
{
	struct pt_regs *regs = current_pt_regs();

	if (!regs) {
		pr_err("kvisor: Failed to get pt_regs\n");
		return -EINVAL;
	}

	pr_info("kvisor: Setting up entry: rip=%lx, rsp=%lx\n", entry, sp);

	/*
	 * Clear all general purpose registers.
	 * This provides a clean slate for the new program.
	 */
	regs->ax = 0;
	regs->bx = 0;
	regs->cx = 0;
	regs->dx = 0;
	regs->si = 0;
	regs->di = 0;
	regs->bp = 0;
	regs->r8 = 0;
	regs->r9 = 0;
	regs->r10 = 0;
	regs->r11 = 0;
	regs->r12 = 0;
	regs->r13 = 0;
	regs->r14 = 0;
	regs->r15 = 0;

	/* Set instruction pointer to entry point */
	regs->ip = entry;

	/* Set stack pointer */
	regs->sp = sp;

	/*
	 * Set up segment registers for user mode.
	 * These should already be set correctly, but make sure.
	 */
	regs->cs = __USER_CS;
	regs->ss = __USER_DS;

	/*
	 * Set flags register to a reasonable default.
	 * Enable interrupts, clear direction flag.
	 */
	regs->flags = X86_EFLAGS_IF;

	return 0;
}
EXPORT_SYMBOL_GPL(kvisor_setup_entry);

/*
 * kvisor_get_pt_regs - Get current pt_regs pointer
 *
 * This is mainly for debugging purposes.
 */
struct pt_regs *kvisor_get_pt_regs(void)
{
	return current_pt_regs();
}
EXPORT_SYMBOL_GPL(kvisor_get_pt_regs);
