/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KSI linker script (arm64)
 *
 * Standard ELF64 aarch64 layout. No EFI header, no hyp sections.
 * Uses asm-generic/vmlinux.lds.h macros for standard section ordering.
 */

#define RO_EXCEPTION_TABLE_ALIGN	4
#define RUNTIME_DISCARD_EXIT

#include <asm-generic/vmlinux.lds.h>
#include <asm/cache.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/thread_info.h>

OUTPUT_ARCH(aarch64)
ENTRY(_start)

jiffies = jiffies_64;

SECTIONS
{
	DISCARDS
	/DISCARD/ : {
		*(.interp .dynamic)
		*(.dynsym .dynstr .hash .gnu.hash)
		*(.ARM.attributes)
	}

	. = KIMAGE_VADDR;

	.head.text : {
		_text = .;
		HEAD_TEXT
	}

	.text : ALIGN(PAGE_SIZE) {
		_stext = .;
		IRQENTRY_TEXT
		SOFTIRQENTRY_TEXT
		ENTRY_TEXT
		TEXT_TEXT
		SCHED_TEXT
		LOCK_TEXT
		KPROBES_TEXT
		*(.gnu.warning)
	}

	/* idmap text - empty for KSI but symbols needed by arm64 code */
	. = ALIGN(PAGE_SIZE);
	__idmap_text_start = .;
	.idmap.text : { *(.idmap.text) }
	__idmap_text_end = .;

	. = ALIGN(PAGE_SIZE);
	_etext = .;

	RO_DATA(PAGE_SIZE)

	/* Alternative instructions patching */
	.altinstructions : {
		__alt_instructions = .;
		*(.altinstructions)
		__alt_instructions_end = .;
	}
	.altinstr_replacement : { *(.altinstr_replacement) }

	/* Relocation sections */
	.rela.dyn : ALIGN(8) {
		*(.rela .rela*)
	}

	.plt : { *(.plt) *(.plt.*) *(.iplt) }
	.igot.plt : { *(.igot.plt) }

	.got : { *(.got) }
	.got.plt : { *(.got.plt) }

	. = ALIGN(PAGE_SIZE);
	__init_begin = .;
	__inittext_begin = .;

	INIT_TEXT_SECTION(8)

	__exittext_begin = .;
	.exit.text : {
		EXIT_TEXT
	}
	__exittext_end = .;

	. = ALIGN(PAGE_SIZE);
	__inittext_end = .;
	__initdata_begin = .;

	.init.data : {
		INIT_DATA
		INIT_SETUP(16)
		INIT_CALLS
		CON_INITCALL
		INIT_RAM_FS
	}
	.exit.data : {
		EXIT_DATA
	}

	PERCPU_SECTION(L1_CACHE_BYTES)

	. = ALIGN(PAGE_SIZE);
	__initdata_end = .;
	__init_end = .;

	_data = .;
	_sdata = .;
	RW_DATA(L1_CACHE_BYTES, PAGE_SIZE, THREAD_ALIGN)

	RUNTIME_CONST_VARIABLES

	/* arm64 mmu code uses this section for data accessed with MMU off */
	.mmuoff.data.write : ALIGN(PAGE_SIZE) {
		*(.mmuoff.data.write)
	}

	_edata = .;

	BSS_SECTION(0, 0, 0)

	. = ALIGN(PAGE_SIZE);
	_end = .;

	STABS_DEBUG
	DWARF_DEBUG
	ELF_DETAILS

}

ASSERT(_text == KIMAGE_VADDR, "HEAD is misaligned")
