/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * KSI kernel entry point (arm64)
 *
 * Minimal entry: set up stack, clear BSS, call ksi_start_kernel().
 * No MMU setup, no EL transition, no cache configuration.
 * The host provides a fully set up environment.
 */

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/assembler.h>
#include <asm/memory.h>
#include <asm/thread_info.h>

	__HEAD
	/*
	 * KSI entry point. The host jumps here after loading the kernel
	 * ELF image. MMU and caches are managed by the host.
	 */
SYM_CODE_START(_start)
	/* Set up the initial stack pointer */
	adrp	x0, init_thread_union
	add	x0, x0, #THREAD_SIZE
	mov	sp, x0

	/* Clear BSS */
	adrp	x0, __bss_start
	adrp	x1, __bss_stop
1:	cmp	x0, x1
	b.ge	2f
	str	xzr, [x0], #8
	b	1b
2:
	/* Jump to C code */
	bl	ksi_start_kernel

	/* Should not return */
	b	.
SYM_CODE_END(_start)
