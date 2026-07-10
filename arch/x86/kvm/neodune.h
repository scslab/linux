/* SPDX-License-Identifier: GPL-2.0 */
/*
 * neodune - Dune-like process virtualization implemented as a KVM VM type
 * (AMD SVM). In-tree interface between neodune.c and the rest of KVM.
 */
#ifndef ARCH_X86_KVM_NEODUNE_H
#define ARCH_X86_KVM_NEODUNE_H

#include <linux/types.h>

struct neodune_caps {
	bool svm;         /* SVM present and not firmware-disabled */
	bool npt;         /* nested paging (the neodune memory model needs this) */
	bool nrips;       /* next-RIP save: VMCB provides RIP after an intercept */
	bool flushbyasid; /* selective TLB flush by ASID */
	u32  asid_max;    /* number of ASIDs (CPUID Fn8000_000A:EBX) */
};

void neodune_probe_caps(struct neodune_caps *c);

/* True iff this CPU can host neodune guests (AMD SVM + NPT). */
bool neodune_supported(void);

#endif /* ARCH_X86_KVM_NEODUNE_H */
