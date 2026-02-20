/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_TLBFLUSH_H
#define __ASM_KSI_TLBFLUSH_H

/*
 * KSI TLB flush operations.
 *
 * For the skeleton, reuse arm64's tlbflush.h to allow compilation.
 * A future KSI implementation will route TLB flushes through
 * ksi_as_unmap() or make them no-ops.
 */

#include <../../../arch/arm64/include/asm/tlbflush.h>

#endif /* __ASM_KSI_TLBFLUSH_H */
