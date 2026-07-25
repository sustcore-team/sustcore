# libtayclib object and archive build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
tayclib-makefile := $(lastword $(MAKEFILE_LIST))
tayclib-dir := $(patsubst %/,%,$(dir $(abspath $(tayclib-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(tayclib-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
-include $(path-cache)/deps-tayclib.mk

tayclib-root := $(path-e)/libs/tayclib
tayclib-obj-root := $(path-obj)/libs/tayclib
tayclib-target ?= $(path-bin)/libs/$(arch)/libtayclib.a

src-root := $(tayclib-root)
obj-root := $(tayclib-obj-root)
target := $(tayclib-target)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
flags-c += $($(arch)-flags-c)

includes-c += $(or $(tayclib-includes-c-$(arch)),$(tayclib-includes-c))
includes-cpp += $(or $(tayclib-includes-cpp-$(arch)),$(tayclib-includes-cpp))
includes-asm += $(or $(tayclib-includes-asm-$(arch)),$(tayclib-includes-asm))
includes-c += -I$(tayclib-root)/include

component-root := $(tayclib-root)
include $(path-s)/build/collector.mk

sources-c := $(strip $(sources-y-c))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects := $(objects-c)
deps := $(objects:.o=.d)

include $(path-s)/rules/c.mk
include $(path-s)/rules/ar.mk

-include $(deps)

.PHONY: build-static tayclib-vars
build-static: $(target)

tayclib-vars:
	$(q)$(echo) "tayclib target=$(target)"
	$(q)$(echo) "tayclib objects=$(objects)"
