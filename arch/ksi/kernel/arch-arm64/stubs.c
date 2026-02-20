// SPDX-License-Identifier: GPL-2.0-only
/*
 * KSI arm64 link stubs
 *
 * Provides stub symbols needed by arm64 code that references
 * hardware-specific objects we've filtered out (page tables, vdso,
 * cpu_ops, pi/).  These will be replaced with real implementations
 * as KSI subsystems are brought up.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm_types.h>
#include <linux/binfmts.h>
#include <asm/page.h>
#include <asm/cpu_ops.h>
#include <asm/vdso.h>

/* Forward declarations */
int arch_setup_additional_pages(struct linux_binprm *bprm, int uses_interp);
void __pi_map_range(void);

/* Page tables - arm64 normally defines these in head.S */
pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
pgd_t reserved_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
pgd_t idmap_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);

/* vdso - arm64 normally defines these in vdso-wrap.S */
char vdso_start[1];
char vdso_end[1];

/* arch_setup_additional_pages - sets up vdso mapping for new processes */
int arch_setup_additional_pages(struct linux_binprm *bprm, int uses_interp)
{
	/* No vdso mapping in KSI skeleton */
	return 0;
}

/* cpu_ops for filtered-out boot methods */
const struct cpu_operations smp_spin_table_ops = {};
const struct cpu_operations cpu_psci_ops = {};

/* pi/ symbols - normally provided by position-independent boot code */
void __pi_map_range(void)
{
	panic("__pi_map_range: not implemented");
}
