// SPDX-License-Identifier: GPL-2.0
/*
 * neodune - Dune-like process virtualization built as a KVM VM type on AMD SVM.
 *
 * neodune runs an ordinary Linux process as its own guest at CPL0, with a
 * nested page table mirroring current->mm and system calls forwarded to the
 * host kernel, reusing KVM's SVM run loop and TDP MMU. This file probes AMD
 * SVM/NPT support and advertises it to userspace via KVM_CAP_NEODUNE.
 *
 * This code is linked into kvm.ko and is called from x86.c within the same
 * module. It must not export any symbols -- the KVM build forbids stray
 * exports under arch/x86/kvm (see the guard in arch/x86/kvm/Makefile).
 */
#define pr_fmt(fmt) "neodune: " fmt

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>

#include <asm/cpufeature.h>
#include <asm/cpufeatures.h>
#include <asm/cpuid/api.h>
#include <asm/msr.h>
#include <asm/msr-index.h>
#include <asm/processor.h>

#include "neodune.h"

void neodune_probe_caps(struct neodune_caps *c)
{
	u64 vmcr = 0;
	u32 eax, ebx, ecx, edx;

	memset(c, 0, sizeof(*c));

	if (!boot_cpu_has(X86_FEATURE_SVM))
		return;

	/* SVM can be disabled (and locked) by firmware. */
	if (!rdmsrq_safe(MSR_VM_CR, &vmcr) && (vmcr & SVM_VM_CR_SVM_DIS_MASK))
		return;

	c->svm         = true;
	c->npt         = boot_cpu_has(X86_FEATURE_NPT);
	c->nrips       = boot_cpu_has(X86_FEATURE_NRIPS);
	c->flushbyasid = boot_cpu_has(X86_FEATURE_FLUSHBYASID);

	/* CPUID Fn8000_000A:EBX = NASID, the number of available ASIDs. */
	if (boot_cpu_data.extended_cpuid_level >= 0x8000000a) {
		cpuid(0x8000000a, &eax, &ebx, &ecx, &edx);
		c->asid_max = ebx;
	}
}

bool neodune_supported(void)
{
	struct neodune_caps c;

	neodune_probe_caps(&c);
	pr_info_once("SVM=%d NPT=%d nRIP-save=%d flush-by-ASID=%d ASIDs=%u\n",
		     c.svm, c.npt, c.nrips, c.flushbyasid, c.asid_max);

	/* neodune's memory model requires nested paging. */
	return c.svm && c.npt;
}
