/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_MMU_CONTEXT_H
#define __ASM_KSI_MMU_CONTEXT_H

/*
 * KSI MMU context switching.
 *
 * For the skeleton, reuse arm64's mmu_context.h to allow compilation.
 * A future KSI implementation will use ksi_as_*() calls instead of
 * TTBR writes.
 */

#include <../../../arch/arm64/include/asm/mmu_context.h>

#endif /* __ASM_KSI_MMU_CONTEXT_H */
