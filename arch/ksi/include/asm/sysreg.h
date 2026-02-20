/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_SYSREG_H
#define __ASM_KSI_SYSREG_H

/*
 * KSI system register access.
 *
 * Reuse arm64's sysreg definitions and accessors. In a full KSI
 * implementation, sysreg reads/writes would be routed through KSI
 * calls, but for the skeleton we keep arm64's definitions intact
 * to allow compilation.
 */

#include <../../../arch/arm64/include/asm/sysreg.h>

#endif /* __ASM_KSI_SYSREG_H */
