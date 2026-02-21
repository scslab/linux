# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the KSI kernel (generic, architecture-independent)
#
# Architecture-specific objects are built in kernel/arch-$(KSI_SRCARCH)/.
#

# Generic KSI kernel objects
obj-y += pv_calls.o
obj-y += setup.o
obj-y += irq.o
obj-y += time.o
obj-y += early_console.o

# Linker script (dispatches to arch-specific .lds.S)
extra-y += vmlinux.lds
