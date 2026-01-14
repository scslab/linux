// SPDX-License-Identifier: GPL-2.0
/*
 * kVisor process helpers
 *
 * These functions provide process setup capabilities for the Rust kVisor
 * implementation.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>
#include <asm/ptrace.h>
#include <asm/processor.h>

#include "interpose.h"
#include "fork_helper.h"

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
 * kvisor_mark_sandboxed - Mark the current process as sandboxed
 */
void kvisor_mark_sandboxed(void)
{
	kvisor_set_sandboxed(current);
	pr_info("kvisor: process %d marked as sandboxed\n", current->pid);
}
EXPORT_SYMBOL_GPL(kvisor_mark_sandboxed);

/*
 * kvisor_get_current_pid - Get the PID of the current process
 */
int kvisor_get_current_pid(void)
{
	return current->pid;
}
EXPORT_SYMBOL_GPL(kvisor_get_current_pid);

/*
 * kvisor_kill_current - Kill the current process
 *
 * This sends SIGKILL to the current process, which will terminate it.
 * This function does not return.
 */
void kvisor_kill_current(void)
{
	do_exit(SIGKILL);
}
EXPORT_SYMBOL_GPL(kvisor_kill_current);
