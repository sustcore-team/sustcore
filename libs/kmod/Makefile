# libkmod runtime archive build skeleton.
ifeq ($(origin global-env), undefined)
kmod-makefile := $(lastword $(MAKEFILE_LIST))
kmod-dir := $(patsubst %/,%,$(dir $(abspath $(kmod-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(kmod-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
-include $(path-cache)/deps-kmod.mk

kmod-root := $(path-e)/libs/kmod
kmod-obj-root := $(path-obj)/libs/kmod
kmod-target ?= $(path-bin)/libs/libkmod.a

src-root := $(kmod-root)
obj-root := $(kmod-obj-root)
target := $(kmod-target)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
comp-cpp ?= $($(arch)-comp-cpp)
comp-asm ?= $(comp-c)
flags-c += $($(arch)-flags-c)
flags-cpp += $($(arch)-flags-cpp)
flags-asm += $($(arch)-flags-c)

includes-c += $(or $(kmod-includes-c-$(arch)),$(kmod-includes-c))
includes-cpp += $(or $(kmod-includes-cpp-$(arch)),$(kmod-includes-cpp))
includes-asm += $(or $(kmod-includes-asm-$(arch)),$(kmod-includes-asm))

sources-asm := arch/$(arch)/crt0.S arch/$(arch)/crti.S arch/$(arch)/crtn.S
sources-cpp := setup.cpp

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
