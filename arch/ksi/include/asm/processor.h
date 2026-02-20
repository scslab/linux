/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_PROCESSOR_H
#define __ASM_KSI_PROCESSOR_H

/*
 * KSI processor definitions.
 *
 * For the skeleton, reuse arm64's processor.h to allow compilation.
 * A future KSI implementation will override cpu_relax() to use
 * ksi_yield() instead of WFE.
 */

#include <../../../arch/arm64/include/asm/processor.h>

#endif /* __ASM_KSI_PROCESSOR_H */
