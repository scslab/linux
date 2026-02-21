/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_TLBFLUSH_H
#define __ASM_KSI_TLBFLUSH_H

/*
 * KSI TLB flush operations.
 *
 * The host manages the real TLB.  All TLBI instructions (EL1-only)
 * are removed.  DSB/ISB barriers are kept as they are unprivileged
 * and maintain memory ordering for software page table operations.
 *
 * Future: route flushes through ksi_as_unmap() to notify the host
 * of mapping invalidations.
 */

#ifndef __ASSEMBLY__

#include <linux/bitfield.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/mmu_notifier.h>
#include <asm/cputype.h>
#include <asm/mmu.h>

/*
 * No-op TLBI macros.  These are referenced by some arm64 code outside
 * of tlbflush.h (e.g. KVM), so provide them as no-ops.
 */
#define __tlbi(op, ...)		do { } while (0)
#define __tlbi_user(op, arg)	do { } while (0)

#define TLBI_TTL_UNKNOWN	INT_MAX

/*
 * TLBI range constants — referenced by kvm_nested.h and other arm64 headers.
 * Keep the definitions even though TLBI instructions are no-ops in KSI.
 */
#define TLBIR_ASID_MASK		GENMASK_ULL(63, 48)
#define TLBIR_TG_MASK		GENMASK_ULL(47, 46)
#define TLBIR_SCALE_MASK	GENMASK_ULL(45, 44)
#define TLBIR_NUM_MASK		GENMASK_ULL(43, 39)
#define TLBIR_TTL_MASK		GENMASK_ULL(38, 37)
#define TLBIR_BADDR_MASK	GENMASK_ULL(36,  0)

#define __TLBI_RANGE_PAGES(num, scale)	\
	((unsigned long)((num) + 1) << (5 * (scale) + 1))
#define MAX_TLBI_RANGE_PAGES		__TLBI_RANGE_PAGES(31, 3)

#define flush_tlb_fix_spurious_fault(vma, address, ptep) do { } while (0)

static inline void local_flush_tlb_all(void)
{
	dsb(nshst);
	dsb(nsh);
	isb();
}

static inline void flush_tlb_all(void)
{
	dsb(ishst);
	dsb(ish);
	isb();
}

static inline void flush_tlb_mm(struct mm_struct *mm)
{
	dsb(ishst);
	dsb(ish);
	mmu_notifier_arch_invalidate_secondary_tlbs(mm, 0, -1UL);
}

static inline void __flush_tlb_page_nosync(struct mm_struct *mm,
					   unsigned long uaddr)
{
	dsb(ishst);
	mmu_notifier_arch_invalidate_secondary_tlbs(mm, uaddr & PAGE_MASK,
						(uaddr & PAGE_MASK) + PAGE_SIZE);
}

static inline void flush_tlb_page_nosync(struct vm_area_struct *vma,
					 unsigned long uaddr)
{
	return __flush_tlb_page_nosync(vma->vm_mm, uaddr);
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long uaddr)
{
	flush_tlb_page_nosync(vma, uaddr);
	dsb(ish);
}

static inline bool arch_tlbbatch_should_defer(struct mm_struct *mm)
{
	return true;
}

static inline void arch_tlbbatch_flush(struct arch_tlbflush_unmap_batch *batch)
{
	dsb(ish);
}

#define MAX_DVM_OPS	PTRS_PER_PTE

static inline void __flush_tlb_range_nosync(struct mm_struct *mm,
				     unsigned long start, unsigned long end,
				     unsigned long stride, bool last_level,
				     int tlb_level)
{
	dsb(ishst);
	mmu_notifier_arch_invalidate_secondary_tlbs(mm, start, end);
}

static inline void __flush_tlb_range(struct vm_area_struct *vma,
				     unsigned long start, unsigned long end,
				     unsigned long stride, bool last_level,
				     int tlb_level)
{
	__flush_tlb_range_nosync(vma->vm_mm, start, end, stride,
				 last_level, tlb_level);
	dsb(ish);
}

static inline void flush_tlb_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end)
{
	__flush_tlb_range(vma, start, end, PAGE_SIZE, false, TLBI_TTL_UNKNOWN);
}

static inline void flush_tlb_kernel_range(unsigned long start, unsigned long end)
{
	dsb(ishst);
	dsb(ish);
	isb();
}

static inline void __flush_tlb_kernel_pgtable(unsigned long kaddr)
{
	dsb(ishst);
	dsb(ish);
	isb();
}

static inline void arch_tlbbatch_add_pending(struct arch_tlbflush_unmap_batch *batch,
		struct mm_struct *mm, unsigned long start, unsigned long end)
{
	__flush_tlb_range_nosync(mm, start, end, PAGE_SIZE, true, 3);
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#define __HAVE_ARCH_FLUSH_PMD_TLB_RANGE
#define flush_pmd_tlb_range(vma, addr, end)	\
	__flush_tlb_range(vma, addr, end, PMD_SIZE, false, 2)
#define flush_pud_tlb_range(vma, addr, end)	\
	__flush_tlb_range(vma, addr, end, PUD_SIZE, false, 1)
#endif /* CONFIG_TRANSPARENT_HUGEPAGE */

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_KSI_TLBFLUSH_H */
