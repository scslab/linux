/* SPDX-License-Identifier: GPL-2.0 */
/*
 * kVisor entry point helpers
 */

#ifndef _KVISOR_ENTRY_HELPER_H
#define _KVISOR_ENTRY_HELPER_H

struct pt_regs;

/*
 * Set up registers to jump to entry point on syscall return.
 */
int kvisor_setup_entry(unsigned long entry, unsigned long sp);

/*
 * Get current pt_regs pointer (for debugging).
 */
struct pt_regs *kvisor_get_pt_regs(void);

#endif /* _KVISOR_ENTRY_HELPER_H */
