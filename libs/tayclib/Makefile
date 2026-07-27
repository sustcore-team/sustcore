# libtayclib object and archive build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
tayclib-makefile := $(lastword $(MAKEFILE_LIST))
tayclib-dir := $(patsubst %/,%,$(dir $(abspath $(tayclib-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(tayclib-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin ctx), undefined)
$(error libs/tayclib/Makefile must be invoked with ctx from the top-level Makefile)
endif

include $(global-env)
environment ?= freestanding
include $(path-s)/toolchain/environment.mk
$(is-host)-tayclib-buildpath := host-buildpath
$(is-freestanding)-tayclib-buildpath := buildpath
include $(path-s)/env/$(y-tayclib-buildpath).mk
include $(ctx)
-include $(or $(deps-file),$(path-deps)/$(owner-id).mk)

src-root := $(owner-root)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/ar.mk

includes-c += $($(owner-id)-includes-c)
includes-cpp += $($(owner-id)-includes-cpp)
includes-asm += $($(owner-id)-includes-asm)

include $(owner-root)/flags.mk
include $(owner-root)/collect.mk

sources-c := $(strip $(sources-y-c))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects := $(objects-c)
deps := $(objects:.o=.d)

$(is-host)-tayclib-toolchain-stamp := $(toolchain-stamp)
$(objects): $(y-tayclib-toolchain-stamp)

include $(path-s)/rules/c.mk
include $(path-s)/rules/ar.mk

-include $(deps)

.PHONY: build-static tayclib-vars
build-static: $(target)

tayclib-vars:
	$(q)$(echo) "tayclib target=$(target)"
	$(q)$(echo) "tayclib objects=$(objects)"
