/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_CURRENT_H
#define __ASM_KSI_CURRENT_H

/*
 * KSI current task pointer.
 *
 * For the skeleton, reuse arm64's current.h to allow compilation.
 * A future KSI implementation will use a per-cpu variable instead
 * of SP_EL0.
 */

#include <../../../arch/arm64/include/asm/current.h>

#endif /* __ASM_KSI_CURRENT_H */
