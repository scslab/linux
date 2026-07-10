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
#include <asm/prctl.h>
#include <asm/ptrace.h>
#include <asm/segment.h>
#include <asm/syscall.h>
#include <asm/unistd.h>

#include <linux/entry-common.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/idr.h>
#include <linux/kvm_host.h>
#include <linux/mm.h>
#include <linux/mmap_lock.h>
#include <linux/sched.h>
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
 * arch_prctl's FS/GS operations target the guest CPU context rather than the
 * host task, so set the guest segment base in the VMCB directly instead of
 * forwarding. Other arch_prctl codes fall through to the host.
 */
static long neodune_arch_prctl(struct kvm_vcpu *vcpu, int code, unsigned long addr)
{
	int sreg = (code == ARCH_SET_FS || code == ARCH_GET_FS) ?
		   VCPU_SREG_FS : VCPU_SREG_GS;
	struct kvm_segment seg;

	kvm_get_segment(vcpu, &seg, sreg);

	switch (code) {
	case ARCH_SET_FS:
	case ARCH_SET_GS:
		if (addr >= TASK_SIZE_MAX)
			return -EPERM;
		seg.base = addr;
		kvm_set_segment(vcpu, &seg, sreg);
		return 0;
	case ARCH_GET_FS:
	case ARCH_GET_GS:
		return put_user(seg.base, (unsigned long __user *)addr) ?
		       -EFAULT : 0;
	}
	return -EINVAL;
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

	if (nr == __NR_arch_prctl &&
	    (regs.di == ARCH_SET_FS || regs.di == ARCH_SET_GS ||
	     regs.di == ARCH_GET_FS || regs.di == ARCH_GET_GS)) {
		kvm_rax_write(vcpu, neodune_arch_prctl(vcpu, regs.di, regs.si));
		return kvm_skip_emulated_instruction(vcpu);
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
	kvm_x86_call(set_cr4)(vcpu, X86_CR4_PAE | X86_CR4_OSFXSR |
				    X86_CR4_OSXMMEXCPT);
	kvm_x86_call(set_cr0)(vcpu, X86_CR0_PE | X86_CR0_PG | X86_CR0_WP);

	kvm_set_segment(vcpu, &cs, VCPU_SREG_CS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_DS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_ES);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_FS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_GS);
	kvm_set_segment(vcpu, &ds, VCPU_SREG_SS);

	kvm_mmu_reset_context(vcpu);
}

/*
 * Reflect an access neodune can't satisfy: unmapped memory, or a write/execute
 * that violates the backing VMA's permissions. Phase 5 will deliver a guest
 * signal; for now, exit to userspace.
 */
static int neodune_fault(struct kvm_vcpu *vcpu, u64 gpa)
{
	vcpu->run->exit_reason = KVM_EXIT_MMIO;
	vcpu->run->mmio.phys_addr = gpa;
	vcpu->run->mmio.len = 0;
	vcpu->run->mmio.is_write = 0;
	return 0;
}

/*
 * On a nested page fault, back the faulting GPA with a per-VMA identity memslot
 * mirroring the VMA at HVA == GPA. If the GPA is already backed, enforce the
 * slot's R/W/X permissions (writes to a read-only slot and fetches from a
 * no-exec slot fault). Slot creation sleeps and drains SRCU, so drop the vCPU's
 * SRCU read lock around it. Returns 1 to retry the guest, 0 with an exit set on
 * a reflected fault, a negative errno on failure, or NEODUNE_NPF_PASS to let
 * KVM's normal handling proceed.
 */
int neodune_npf(struct kvm_vcpu *vcpu, u64 gpa, u64 error_code)
{
	struct kvm *kvm = vcpu->kvm;
	struct kvm_userspace_memory_region2 m;
	struct kvm_memory_slot *slot;
	struct kvm_memslot_iter iter;
	struct vm_area_struct *vma;
	unsigned long start, end, lo, hi;
	bool writable = false, executable = false, found = false;
	int id, r;

	slot = kvm_vcpu_gfn_to_memslot(vcpu, gpa >> PAGE_SHIFT);
	if (slot) {
		if (((error_code & PFERR_WRITE_MASK) &&
		     (slot->flags & KVM_MEM_READONLY)) ||
		    ((error_code & PFERR_FETCH_MASK) && slot->arch.neodune_noexec))
			return neodune_fault(vcpu, gpa);
		return NEODUNE_NPF_PASS;
	}

	mmap_read_lock(current->mm);
	vma = vma_lookup(current->mm, gpa);
	if (vma) {
		start = vma->vm_start;
		end = vma->vm_end;
		writable = !!(vma->vm_flags & VM_WRITE);
		executable = !!(vma->vm_flags & VM_EXEC);
		found = true;
	}
	mmap_read_unlock(current->mm);

	if (!found)
		return neodune_fault(vcpu, gpa);

	kvm_vcpu_srcu_read_unlock(vcpu);
	mutex_lock(&kvm->slots_lock);

	if (kvm_vcpu_gfn_to_memslot(vcpu, gpa >> PAGE_SHIFT)) {
		r = 1;
		goto out;
	}

	/*
	 * Clamp the new slot to the uncovered gap containing gpa within the VMA,
	 * bounded by neighboring slots, so a grown or merged VMA (whose earlier
	 * range is already backed) does not overlap an existing slot.
	 */
	lo = start;
	hi = end;
	kvm_for_each_memslot_in_gfn_range(&iter, kvm_memslots(kvm),
					  start >> PAGE_SHIFT,
					  (end + PAGE_SIZE - 1) >> PAGE_SHIFT) {
		unsigned long ms = iter.slot->base_gfn << PAGE_SHIFT;
		unsigned long me = ms + (iter.slot->npages << PAGE_SHIFT);

		if (me <= gpa)
			lo = max(lo, me);
		else if (ms > gpa) {
			hi = min(hi, ms);
			break;
		}
	}

	id = ida_alloc_max(&kvm->arch.neodune_ida, KVM_USER_MEM_SLOTS - 1,
			   GFP_KERNEL);
	if (id < 0) {
		r = id;
		goto out;
	}

	memset(&m, 0, sizeof(m));
	m.slot = id;
	m.flags = writable ? 0 : KVM_MEM_READONLY;
	m.guest_phys_addr = lo;
	m.userspace_addr = lo;
	m.memory_size = hi - lo;

	r = kvm_set_memory_region(kvm, &m);
	if (r) {
		ida_free(&kvm->arch.neodune_ida, id);
		goto out;
	}

	slot = id_to_memslot(kvm_memslots(kvm), id);
	if (slot)
		slot->arch.neodune_noexec = !executable;
	r = 1;
out:
	mutex_unlock(&kvm->slots_lock);
	kvm_vcpu_srcu_read_lock(vcpu);
	return r;
}
EXPORT_SYMBOL_FOR_KVM_INTERNAL(neodune_npf);

/*
 * A per-VMA memslot is stale if its VMA is gone, resized, or its R/W/X
 * permissions changed; such slots are deleted and lazily re-created with fresh
 * state on the next access. Internal slots (id >= KVM_USER_MEM_SLOTS, e.g. the
 * identity page table) are never neodune's and are left alone.
 */
static bool neodune_slot_stale(struct kvm_memory_slot *slot)
{
	unsigned long hva = slot->userspace_addr;
	unsigned long size = slot->npages << PAGE_SHIFT;
	struct vm_area_struct *vma;
	bool stale = true;

	if (slot->id >= KVM_USER_MEM_SLOTS)
		return false;

	mmap_read_lock(current->mm);
	vma = vma_lookup(current->mm, hva);
	/*
	 * Keep the slot if its VMA still covers it with matching perms — a grown
	 * or merged VMA (vm_end past the slot) is fine, only a shrink (split) or a
	 * permission change makes it stale.
	 */
	if (vma && vma->vm_end >= hva + size &&
	    !!(vma->vm_flags & VM_WRITE) == !(slot->flags & KVM_MEM_READONLY) &&
	    !!(vma->vm_flags & VM_EXEC) == !slot->arch.neodune_noexec)
		stale = false;
	mmap_read_unlock(current->mm);
	return stale;
}

static void neodune_delete_slot(struct kvm *kvm, int id)
{
	struct kvm_userspace_memory_region2 m = { .slot = id };

	if (!kvm_set_memory_region(kvm, &m))
		ida_free(&kvm->arch.neodune_ida, id);
}

/*
 * Record an invalidated GPA range (neodune slots are identity, so the notifier
 * HVA range is the GPA range) and kick the vCPUs to reconcile. Runs in the
 * mmu_notifier under mmap_lock, so it may not sleep or touch memslots here.
 */
void neodune_note_invalidate(struct kvm *kvm, u64 start, u64 end)
{
	if (!kvm->arch.neodune)
		return;

	spin_lock(&kvm->arch.neodune_lock);
	if (kvm->arch.neodune_inval_start >= kvm->arch.neodune_inval_end) {
		kvm->arch.neodune_inval_start = start;
		kvm->arch.neodune_inval_end = end;
	} else {
		kvm->arch.neodune_inval_start =
			min(kvm->arch.neodune_inval_start, start);
		kvm->arch.neodune_inval_end =
			max(kvm->arch.neodune_inval_end, end);
	}
	spin_unlock(&kvm->arch.neodune_lock);

	kvm_make_all_cpus_request(kvm, KVM_REQ_NEODUNE_SYNC);
}

#define NEODUNE_SYNC_BATCH 16

void neodune_sync_memslots(struct kvm_vcpu *vcpu)
{
	struct kvm *kvm = vcpu->kvm;
	struct kvm_memslot_iter iter;
	struct kvm_memslots *slots;
	int ids[NEODUNE_SYNC_BATCH];
	gfn_t start, end;
	u64 s, e;
	int nr, i;

	spin_lock(&kvm->arch.neodune_lock);
	s = kvm->arch.neodune_inval_start;
	e = kvm->arch.neodune_inval_end;
	kvm->arch.neodune_inval_start = 0;
	kvm->arch.neodune_inval_end = 0;
	spin_unlock(&kvm->arch.neodune_lock);

	if (s >= e)
		return;

	start = s >> PAGE_SHIFT;
	end = (e + PAGE_SIZE - 1) >> PAGE_SHIFT;

	kvm_vcpu_srcu_read_unlock(vcpu);
	mutex_lock(&kvm->slots_lock);

	/* The iterator is invalidated by a delete, so collect ids then delete. */
	do {
		nr = 0;
		slots = kvm_memslots(kvm);
		kvm_for_each_memslot_in_gfn_range(&iter, slots, start, end) {
			if (neodune_slot_stale(iter.slot))
				ids[nr++] = iter.slot->id;
			if (nr == NEODUNE_SYNC_BATCH)
				break;
		}
		for (i = 0; i < nr; i++)
			neodune_delete_slot(kvm, ids[i]);
	} while (nr == NEODUNE_SYNC_BATCH);

	mutex_unlock(&kvm->slots_lock);
	kvm_vcpu_srcu_read_lock(vcpu);
}
