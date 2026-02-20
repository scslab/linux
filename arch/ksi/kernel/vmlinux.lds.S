/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KSI linker script dispatcher.
 * Includes the sub-architecture-specific linker script.
 */

#ifdef CONFIG_ARM64
#include "arch-arm64/vmlinux.lds.S"
#endif
