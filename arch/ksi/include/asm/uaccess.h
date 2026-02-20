/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_KSI_UACCESS_H
#define __ASM_KSI_UACCESS_H

/*
 * KSI user memory access.
 *
 * Reuse arm64's uaccess implementation for now.
 * A future KSI-specific implementation may walk page tables and access
 * through phys-to-virt, without PAN/TTBR0 manipulation.
 */

#include <../../../arch/arm64/include/asm/uaccess.h>

#endif /* __ASM_KSI_UACCESS_H */
