/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_ALTERNATIVE_H
#define __ASM_KSI_ALTERNATIVE_H

/*
 * KSI alternative/code patching.
 *
 * Reuse arm64's alternative.h for struct alt_instr and macros.
 * Code patching is effectively a no-op in para-virt (no CPU errata),
 * but the data structures must be present for compilation.
 */

#include <../../../arch/arm64/include/asm/alternative.h>

#endif /* __ASM_KSI_ALTERNATIVE_H */
