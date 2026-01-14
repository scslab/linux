/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kVisor process helpers
 */

#ifndef _KVISOR_FORK_HELPER_H
#define _KVISOR_FORK_HELPER_H

/*
 * Set up registers to jump to entry point on syscall return.
 *
 * This modifies the current task's pt_regs so that when the syscall
 * returns, execution will jump to the specified entry point with the
 * given stack pointer.
 *
 * Returns 0 on success, negative error on failure.
 */
int kvisor_setup_entry(unsigned long entry, unsigned long sp);

/*
 * Mark the current process as sandboxed.
 */
void kvisor_mark_sandboxed(void);

/*
 * Get the PID of the current process.
 */
int kvisor_get_current_pid(void);

/*
 * Kill the current process with SIGKILL.
 * This does not return.
 */
void kvisor_kill_current(void);

#endif /* _KVISOR_FORK_HELPER_H */
