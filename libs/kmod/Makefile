# libkmod runtime archive build skeleton.
ifeq ($(origin global-env), undefined)
kmod-makefile := $(lastword $(MAKEFILE_LIST))
kmod-dir := $(patsubst %/,%,$(dir $(abspath $(kmod-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(kmod-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin ctx), undefined)
$(error libs/kmod/Makefile must be invoked with ctx from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
include $(ctx)
-include $(path-deps)/$(owner-id).mk

src-root := $(owner-root)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ar.mk

comp-asm ?= $(comp-c)
flags-asm += $($(arch)-flags-c)

includes-c += $($(owner-id)-includes-c)
includes-cpp += $($(owner-id)-includes-cpp)
includes-asm += $($(owner-id)-includes-asm)

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
