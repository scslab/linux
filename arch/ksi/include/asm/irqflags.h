/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_IRQFLAGS_H
#define __ASM_KSI_IRQFLAGS_H

/*
 * KSI IRQ flag management.
 *
 * For the skeleton, reuse arm64's irqflags to allow compilation.
 * A future KSI implementation will replace MSR DAIF with software
 * flags and ksi_irq_*() calls.
 */

#include <../../../arch/arm64/include/asm/irqflags.h>

#endif /* __ASM_KSI_IRQFLAGS_H */
