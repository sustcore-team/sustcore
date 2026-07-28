# Kernel image layer built on the common component compilation rules.
ifeq ($(origin global-env), undefined)
$(error kernel/Makefile must be invoked with global-env from the top-level Makefile)
endif

kernel-makefile := $(lastword $(MAKEFILE_LIST))
component-root := $(abspath $(dir $(kernel-makefile)))
include $(global-env)

component-config-mks = \
	$(owner-root)/variant.$(arch).mk \
	$(owner-root)/enable.mk
component-extra-objects = $(path-initrd-obj)
include $(path-s)/build/component.mk

link-script ?= $(boot-link-script)
archives += $(component-dep-archives)

include $(path-s)/toolchain/ld.mk

riscv64-objcopy-output := elf64-littleriscv
riscv64-objcopy-arch := riscv
loongarch64-objcopy-output := elf64-loongarch
loongarch64-objcopy-arch := loongarch

comp-objcopy ?= llvm-objcopy

include $(path-s)/rules/ld.mk

$(path-initrd-obj): $(path-initrd)
	$(q)$(mkdir) $(@D)
	$(q)$(comp-objcopy) -I binary -O $($(arch)-objcopy-output) \
		-B $($(arch)-objcopy-arch) --rename-section .data=.attach.initrd $< $@

.PHONY: build kernel-vars
build: $(target)

kernel-vars:
	$(q)$(echo) "kernel arch=$(arch) boot=$(if $(filter y,$(enable-sbi)),sbi,laboot)"
	$(q)$(echo) "kernel target=$(target)"
	$(q)$(echo) "kernel link-script=$(link-script)"
	$(q)$(echo) "kernel asm sources=$(sources-asm)"
	$(q)$(echo) "kernel cpp sources=$(sources-cpp)"
	$(q)$(echo) "kernel objects=$(objects)"
