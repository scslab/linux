/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_PERCPU_H
#define __ASM_KSI_PERCPU_H

/*
 * KSI per-cpu offset management.
 *
 * For the skeleton, reuse arm64's percpu.h to allow compilation.
 * A future KSI implementation will use a global array instead of
 * TPIDR_EL1.
 */

#include <../../../arch/arm64/include/asm/percpu.h>

#endif /* __ASM_KSI_PERCPU_H */
