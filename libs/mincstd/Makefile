# libmini-cstd object and archive build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
minicstd-makefile := $(lastword $(MAKEFILE_LIST))
minicstd-dir := $(patsubst %/,%,$(dir $(abspath $(minicstd-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(minicstd-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin build-header), undefined)
$(error libs/mincstd/Makefile must be invoked with build-header from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
include $(build-header)
-include $(path-cache)/deps-$(owner-id).mk

src-root := $(owner-root)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
flags-c += $($(arch)-flags-c)

includes-c += $(or $($(owner-id)-includes-c-$(arch)),$($(owner-id)-includes-c))
includes-cpp += $(or $($(owner-id)-includes-cpp-$(arch)),$($(owner-id)-includes-cpp))
includes-asm += $(or $($(owner-id)-includes-asm-$(arch)),$($(owner-id)-includes-asm))

include $(owner-root)/flags.mk
include $(owner-root)/collect.mk

sources-c := $(strip $(sources-y-c))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects := $(objects-c)
deps := $(objects:.o=.d)

include $(path-s)/rules/c.mk
include $(path-s)/rules/ar.mk

-include $(deps)

.PHONY: build-static minicstd-vars
build-static: $(target)

minicstd-vars:
	$(q)$(echo) "mini-cstd target=$(target)"
	$(q)$(echo) "mini-cstd objects=$(objects)"
