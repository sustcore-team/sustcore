# libmini-cstd object and archive build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
minicstd-makefile := $(lastword $(MAKEFILE_LIST))
minicstd-dir := $(patsubst %/,%,$(dir $(abspath $(minicstd-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(minicstd-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
-include $(path-cache)/deps-mini-cstd.mk

minicstd-root := $(path-e)/libs/mincstd
minicstd-obj-root := $(path-obj)/libs/mincstd
minicstd-target ?= $(path-bin)/libs/$(arch)/libmini-cstd.a

src-root := $(minicstd-root)
obj-root := $(minicstd-obj-root)
target := $(minicstd-target)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
flags-c += $($(arch)-flags-c)

includes-c += $(or $(mini-cstd-includes-c-$(arch)),$(mini-cstd-includes-c))
includes-cpp += $(or $(mini-cstd-includes-cpp-$(arch)),$(mini-cstd-includes-cpp))
includes-asm += $(or $(mini-cstd-includes-asm-$(arch)),$(mini-cstd-includes-asm))

include $(minicstd-root)/flags.mk
include $(minicstd-root)/include.mk

sources-c := $(strip $(sources-c))
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
