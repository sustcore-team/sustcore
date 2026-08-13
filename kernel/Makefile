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
include $(path-s)/build/component.mk

link-script ?= $(boot-link-script)
archives += $(component-dep-archives)

include $(path-s)/toolchain/ld.mk
include $(path-s)/rules/attachment.mk
include $(path-s)/rules/ld.mk

.PHONY: build kernel-vars
build: $(target)

kernel-vars:
	$(q)$(echo) "kernel arch=$(arch) boot=$(if $(filter y,$(enable-sbi)),sbi,laboot)"
	$(q)$(echo) "kernel target=$(target)"
	$(q)$(echo) "kernel link-script=$(link-script)"
	$(q)$(echo) "kernel asm sources=$(sources-asm)"
	$(q)$(echo) "kernel cpp sources=$(sources-cpp)"
	$(q)$(echo) "kernel objects=$(objects)"
