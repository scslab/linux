# SPDX-License-Identifier: GPL-2.0

arm64_probes := ../../../../arm64/kernel/probes

obj-$(CONFIG_KPROBES)		+= kprobes.o decode-insn.o	\
				   kprobes_trampoline.o		\
				   simulate-insn.o
obj-$(CONFIG_UPROBES)		+= uprobes.o decode-insn.o	\
				   simulate-insn.o

define ksi_arm64_ref
  $(foreach f,$(1),$(eval $(f)-y := $(arm64_probes)/$(f).o))
endef

$(call ksi_arm64_ref, \
	kprobes decode-insn kprobes_trampoline \
	uprobes simulate-insn \
)
