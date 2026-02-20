/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_CACHEFLUSH_H
#define __ASM_KSI_CACHEFLUSH_H

/*
 * KSI cache flush operations.
 *
 * For the skeleton, reuse arm64's cacheflush.h to allow compilation.
 * A future KSI implementation will make all cache ops no-ops since
 * the host manages cache coherency.
 */

#include <../../../arch/arm64/include/asm/cacheflush.h>

#endif /* __ASM_KSI_CACHEFLUSH_H */
