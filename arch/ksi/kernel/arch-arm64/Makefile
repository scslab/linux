# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the KSI kernel (arm64 sub-architecture)
#
# Strategy: compile arm64's kernel sources directly, overriding only
# what KSI needs to change. Uses composite object rules to reference
# arm64 source files (e.g., traps-y := ../../../arm64/kernel/traps.o).
#

arm64_kern := ../../../arm64/kernel

CFLAGS_armv8_deprecated.o := -I$(srctree)/arch/arm64/kernel
CFLAGS_REMOVE_ftrace.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_insn.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_return_address.o = $(CC_FLAGS_FTRACE)
CFLAGS_REMOVE_syscall.o = -fstack-protector -fstack-protector-strong
CFLAGS_syscall.o += -fno-stack-protector
KASAN_SANITIZE_stacktrace.o := n
KCOV_INSTRUMENT_entry-common.o := n
KCOV_INSTRUMENT_idle.o := n

# Core arm64 kernel objects - same list as arch/arm64/kernel/Makefile
obj-y := debug-monitors.o entry.o irq.o fpsimd.o			\
	 entry-common.o entry-fpsimd.o process.o ptrace.o		\
	 setup.o signal.o sys.o stacktrace.o time.o traps.o		\
	 io.o vdso.o hyp-stub.o psci.o cpu_ops.o			\
	 return_address.o cpuinfo.o cpu_errata.o			\
	 cpufeature.o alternative.o cacheinfo.o				\
	 smp.o smp_spin_table.o topology.o smccc-call.o			\
	 syscall.o proton-pack.o idle.o patching.o pi/			\
	 rsi.o jump_label.o

# KSI: filter out hardware-specific and complex objects not needed for skeleton
obj-y := $(filter-out smp_spin_table.o hyp-stub.o psci.o pi/ vdso.o,$(obj-y))

# Conditional arm64 objects
obj-$(CONFIG_COMPAT)			+= sys32.o signal32.o sys_compat.o
obj-$(CONFIG_COMPAT)			+= sigreturn32.o
obj-$(CONFIG_FUNCTION_TRACER)		+= ftrace.o entry-ftrace.o
obj-$(CONFIG_MODULES)			+= module.o module-plts.o
obj-$(CONFIG_PERF_EVENTS)		+= perf_regs.o perf_callchain.o
obj-$(CONFIG_HARDLOCKUP_DETECTOR_PERF)	+= watchdog_hld.o
obj-$(CONFIG_HAVE_HW_BREAKPOINT)	+= hw_breakpoint.o
obj-$(CONFIG_EFI)			+= efi.o efi-rt-wrapper.o
obj-$(CONFIG_PCI)			+= pci.o
obj-$(CONFIG_ACPI)			+= acpi.o
obj-$(CONFIG_PARAVIRT)			+= paravirt.o
obj-$(CONFIG_ELF_CORE)			+= elfcore.o
obj-$(CONFIG_CRASH_DUMP)		+= crash_dump.o
obj-$(CONFIG_VMCORE_INFO)		+= vmcore_info.o
obj-$(CONFIG_ARM64_PTR_AUTH)		+= pointer_auth.o
obj-$(CONFIG_ARM64_MTE)			+= mte.o

obj-y += probes/

# KSI arm64-specific objects (compiled locally)
obj-y += head.o
obj-y += stubs.o

# All arm64 kernel sources are compiled from arm64's directory.
# Each line creates a composite rule: foo-y := ../../../arm64/kernel/foo.o
# which tells kbuild to compile arch/arm64/kernel/foo.c and place the
# result in arch/ksi/kernel/arch-arm64/foo.o.
define ksi_arm64_ref
  $(foreach f,$(1),$(eval $(f)-y := $(arm64_kern)/$(f).o))
endef

$(call ksi_arm64_ref, \
	debug-monitors entry irq fpsimd \
	entry-common entry-fpsimd process ptrace \
	setup signal sys stacktrace time traps \
	io cpu_ops \
	return_address cpuinfo cpu_errata \
	cpufeature alternative cacheinfo \
	smp topology smccc-call \
	syscall proton-pack idle patching \
	rsi jump_label \
	sys32 signal32 sys_compat sigreturn32 \
	compat_alignment kuser32 \
	ftrace entry-ftrace \
	module module-plts \
	perf_regs perf_callchain \
	watchdog_hld hw_breakpoint \
	efi efi-rt-wrapper pci \
	acpi paravirt \
	elfcore crash_dump vmcore_info \
	pointer_auth mte \
)
