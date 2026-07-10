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
#include <asm/processor-flags.h>
#include <asm/pgtable_types.h>
#include <asm/ptrace.h>
#include <asm/segment.h>
#include <asm/syscall.h>
#include <asm/unistd.h>

#include <linux/entry-common.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/kvm_host.h>
#include <linux/uaccess.h>

#include "neodune.h"
#include "kvm_cache_regs.h"
#include "mmu.h"
#include "x86.h"

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

/*
 * A neodune guest issues system calls with VMMCALL. Marshal the guest registers
 * into a pt_regs and dispatch to the host handler; the call runs against
 * current->mm, and GVA == HVA makes pointer arguments valid host addresses.
 * exit and exit_group leave the guest by returning to userspace.
 */
int neodune_handle_vmmcall(struct kvm_vcpu *vcpu)
{
	unsigned long nr = kvm_rax_read(vcpu);
	struct pt_regs regs = {
		.orig_ax = nr,
		.ax = nr,
		.di = kvm_register_read_raw(vcpu, VCPU_REGS_RDI),
		.si = kvm_register_read_raw(vcpu, VCPU_REGS_RSI),
		.dx = kvm_register_read_raw(vcpu, VCPU_REGS_RDX),
		.r10 = kvm_register_read_raw(vcpu, VCPU_REGS_R10),
		.r8 = kvm_register_read_raw(vcpu, VCPU_REGS_R8),
		.r9 = kvm_register_read_raw(vcpu, VCPU_REGS_R9),
		.ip = kvm_rip_read(vcpu),
		.sp = kvm_register_read_raw(vcpu, VCPU_REGS_RSP),
		.cs = __USER_CS,
		.ss = __USER_DS,
		.flags = X86_EFLAGS_IF | X86_EFLAGS_FIXED,
	};
	long snr, ret;

	if (nr == __NR_exit || nr == __NR_exit_group) {
		vcpu->run->exit_reason = KVM_EXIT_SHUTDOWN;
		return 0;
	}

	/* Compose the host's syscall-entry work (seccomp, ptrace, audit). */
	snr = syscall_enter_from_user_mode_work(&regs, nr);

	if (snr == -1L) {
		ret = regs.ax;			/* skipped by seccomp/ptrace */
	} else if ((unsigned long)snr < NR_syscalls) {
		kvm_vcpu_srcu_read_unlock(vcpu);
		ret = x64_sys_call(&regs, snr);
		kvm_vcpu_srcu_read_lock(vcpu);
	} else {
		ret = -ENOSYS;
	}

	kvm_rax_write(vcpu, ret);
	return kvm_skip_emulated_instruction(vcpu);
}

#define NEODUNE_PT_GPA		0xfe000000ULL
#define NEODUNE_PT_PML4ES	256			/* low canonical half */
#define NEODUNE_PT_PAGES	(1 + NEODUNE_PT_PML4ES)	/* PML4 + PDPTs */
#define NEODUNE_PT_SIZE		(NEODUNE_PT_PAGES * PAGE_SIZE)
#define NEODUNE_IDMAP_SLOT	(KVM_USER_MEM_SLOTS + 2)

int neodune_vm_setup(struct kvm *kvm)
{
	void __user *hva;
	u64 *page;
	int i, j, ret;

	page = (u64 *)__get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	mutex_lock(&kvm->slots_lock);

	hva = __x86_set_memory_region(kvm, NEODUNE_IDMAP_SLOT, NEODUNE_PT_GPA,
				      NEODUNE_PT_SIZE);
	if (IS_ERR(hva)) {
		ret = PTR_ERR(hva);
		goto out;
	}

	/* PML4: entry i -> PDPT page i. */
	memset(page, 0, PAGE_SIZE);
	for (i = 0; i < NEODUNE_PT_PML4ES; i++)
		page[i] = (NEODUNE_PT_GPA + (u64)(1 + i) * PAGE_SIZE) |
			  _PAGE_PRESENT | _PAGE_RW;
	ret = __copy_to_user(hva, page, PAGE_SIZE) ? -EFAULT : 0;
	if (ret)
		goto out;

	/* PDPTs: entry j -> 1 GiB identity page. */
	for (i = 0; i < NEODUNE_PT_PML4ES; i++) {
		for (j = 0; j < 512; j++)
			page[j] = (((u64)i * 512 + j) << 30) |
				  _PAGE_PRESENT | _PAGE_RW | _PAGE_PSE;
		if (__copy_to_user(hva + (u64)(1 + i) * PAGE_SIZE, page,
				   PAGE_SIZE)) {
			ret = -EFAULT;
			goto out;
		}
	}
out:
	mutex_unlock(&kvm->slots_lock);
	free_page((unsigned long)page);
	return ret;
}

void neodune_setup_guest_state(struct kvm_vcpu *vcpu)
{
	struct kvm_segment cs = {
		.limit = 0xfffff, .selector = 0x08, .type = 0xb,
		.s = 1, .present = 1, .l = 1, .g = 1,
	};
	struct kvm_segment ds = {
		.limit = 0xfffff, .selector = 0x10, .type = 0x3,
		.s = 1, .present = 1, .db = 1, .g = 1,
	};

	vcpu->arch.cr2 = 0;
	vcpu->arch.cr3 = NEODUNE_PT_GPA;
	kvm_register_mark_dirty(vcpu, VCPU_EXREG_CR3);
	kvm_x86_call(post_set_cr3)(vcpu, NEODUNE_PT_GPA);

	kvm_x86_call(set_efer)(vcpu, EFER_LME);
	kvm_x86_call(set_cr4)(vcpu, X86_CR4_PAE);
	kvm_x86_call(set_cr0)(vcpu, X86_CR0_PE | X86_CR0_PG | X86_CR0_WP);

	kvm_set_segment(vcpu, &cs, VCPU_SREG_CS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_DS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_ES);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_FS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_GS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_SS);

	kvm_mmu_reset_context(vcpu);
}
