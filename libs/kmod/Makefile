# libkmod runtime archive build skeleton.
ifeq ($(origin global-env), undefined)
kmod-makefile := $(lastword $(MAKEFILE_LIST))
kmod-dir := $(patsubst %/,%,$(dir $(abspath $(kmod-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(kmod-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin build-header), undefined)
$(error libs/kmod/Makefile must be invoked with build-header from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
include $(build-header)
-include $(path-cache)/deps-$(owner-id).mk

src-root := $(owner-root)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
comp-cpp ?= $($(arch)-comp-cpp)
comp-asm ?= $(comp-c)
flags-c += $($(arch)-flags-c)
flags-cpp += $($(arch)-flags-cpp)
flags-asm += $($(arch)-flags-c)

includes-c += $(or $($(owner-id)-includes-c-$(arch)),$($(owner-id)-includes-c))
includes-cpp += $(or $($(owner-id)-includes-cpp-$(arch)),$($(owner-id)-includes-cpp))
includes-asm += $(or $($(owner-id)-includes-asm-$(arch)),$($(owner-id)-includes-asm))

include $(owner-root)/flags.mk
include $(owner-root)/collect.mk

sources-asm := $(strip $(sources-y-asm))
sources-cpp := $(strip $(sources-y-cpp))

objects-asm := $(addprefix $(obj-root)/,$(sources-asm:.S=.o))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects := $(objects-asm) $(objects-cpp)
deps := $(objects:.o=.d)

include $(path-s)/rules/asm.mk
include $(path-s)/rules/cpp.mk
include $(path-s)/rules/ar.mk

-include $(deps)

.PHONY: build-static kmod-vars
build-static: $(target)

kmod-vars:
	$(q)$(echo) "kmod target=$(target)"
	$(q)$(echo) "kmod objects=$(objects)"
