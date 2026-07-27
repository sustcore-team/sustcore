# libsbi object build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
libsbi-makefile := $(lastword $(MAKEFILE_LIST))
libsbi-dir := $(patsubst %/,%,$(dir $(abspath $(libsbi-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(libsbi-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin ctx), undefined)
$(error libs/sbi/Makefile must be invoked with ctx from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
include $(ctx)
-include $(path-deps)/$(owner-id).mk

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

include $(path-s)/rules/c.mk
include $(path-s)/rules/ar.mk

-include $(deps)

.PHONY: build-static libsbi-vars
build-static: $(target)

libsbi-vars:
	$(q)$(echo) "libsbi target=$(target)"
	$(q)$(echo) "libsbi objects=$(objects)"
