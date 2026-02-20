# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the KSI memory management (arm64 sub-architecture)
#
# Compile arm64's mm sources directly.
#

arm64_mm := ../../../arm64/mm

# Core arm64 mm objects - same list as arch/arm64/mm/Makefile
obj-y				:= dma-mapping.o extable.o fault.o init.o \
				   cache.o copypage.o flush.o \
				   ioremap.o mmap.o pgd.o mem_encrypt.o mmu.o \
				   context.o proc.o pageattr.o fixmap.o
obj-$(CONFIG_ARM64_CONTPTE)	+= contpte.o
obj-$(CONFIG_HUGETLB_PAGE)	+= hugetlbpage.o
obj-$(CONFIG_PTDUMP)		+= ptdump.o
obj-$(CONFIG_PTDUMP_DEBUGFS)	+= ptdump_debugfs.o
obj-$(CONFIG_TRANS_TABLE)	+= trans_pgd.o
obj-$(CONFIG_TRANS_TABLE)	+= trans_pgd-asm.o
obj-$(CONFIG_DEBUG_VIRTUAL)	+= physaddr.o
obj-$(CONFIG_ARM64_MTE)		+= mteswap.o
obj-$(CONFIG_ARM64_GCS)		+= gcs.o
KASAN_SANITIZE_physaddr.o	+= n

obj-$(CONFIG_KASAN)		+= kasan_init.o
KASAN_SANITIZE_kasan_init.o	:= n

# Reference all arm64 mm sources
define ksi_arm64_ref
  $(foreach f,$(1),$(eval $(f)-y := $(arm64_mm)/$(f).o))
endef

$(call ksi_arm64_ref, \
	dma-mapping extable fault init \
	cache copypage flush \
	ioremap mmap pgd mem_encrypt mmu \
	context proc pageattr fixmap \
	contpte hugetlbpage \
	ptdump ptdump_debugfs \
	trans_pgd trans_pgd-asm \
	physaddr mteswap gcs \
	kasan_init \
)
