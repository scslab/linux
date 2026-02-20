#
# arch/ksi/Makefile
#
# KSI (Kernel System Interface) architecture build file.
# Reuses sub-architecture code via include path injection.
#

KBUILD_DEFCONFIG := ksi_defconfig

# Sub-architecture selection. Currently only arm64 is supported.
# To add a new sub-architecture, add an ifeq block here and create
# the corresponding kernel/arch-<arch>/ and mm/arch-<arch>/ directories.
KSI_SRCARCH := arm64
export KSI_SRCARCH

# --- arm64-specific flags ---
# These will move to a conditional block when more architectures are added.

UTS_MACHINE := aarch64

LDFLAGS_vmlinux := --no-undefined -X

KBUILD_CFLAGS	+= -mgeneral-regs-only
KBUILD_CFLAGS	+= $(call cc-option,-mabi=lp64)
KBUILD_AFLAGS	+= $(call cc-option,-mabi=lp64)

KBUILD_CFLAGS	+= -fno-asynchronous-unwind-tables -fno-unwind-tables
KBUILD_AFLAGS	+= -fno-asynchronous-unwind-tables -fno-unwind-tables

ifeq ($(CONFIG_AS_HAS_ARMV8_5), y)
  asm-arch := armv8.5-a
else
  asm-arch := armv8.4-a
endif

KBUILD_CFLAGS	+= -Wa,-march=$(asm-arch) \
		   -DARM64_ASM_ARCH='"$(asm-arch)"'
KBUILD_CFLAGS	+= $(call cc-option,-mbranch-protection=none)

KBUILD_CPPFLAGS	+= -mlittle-endian
CHECKFLAGS	+= -D__AARCH64EL__
KBUILD_LDFLAGS	+= -EL $(call ld-option, -maarch64elf, -maarch64linux -z norelro)

CHECKFLAGS	+= -D__aarch64__

# --- End arm64-specific flags ---

# Include path: arch/ksi/include first, then sub-arch includes as fallback
LINUXINCLUDE	:= \
	-I$(srctree)/arch/ksi/include \
	-I$(srctree)/arch/$(KSI_SRCARCH)/include \
	-I$(srctree)/arch/$(KSI_SRCARCH)/include/uapi \
	$(LINUXINCLUDE)

# Library path - reuse sub-architecture's lib
libs-y		:= arch/$(KSI_SRCARCH)/lib/ $(libs-y)

# Head object - from the arch-specific subdirectory
head-y		:= arch/ksi/kernel/arch-$(KSI_SRCARCH)/head.o

# Default image is just vmlinux (standard ELF)
KBUILD_IMAGE	:= vmlinux

# Generate sub-architecture's build-time headers and stub vdso-offsets.h
archprepare:
	$(Q)$(MAKE) $(build)=arch/arm64/tools kapi
	$(Q)echo '/* Stub vdso-offsets.h for KSI */' > $(objtree)/include/generated/vdso-offsets.h
	$(Q)echo '#define vdso_offset_sigtramp 0' >> $(objtree)/include/generated/vdso-offsets.h

all:	vmlinux

define archhelp
  echo  '  vmlinux	- Standard ELF kernel image'
endef
