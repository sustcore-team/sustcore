# libsbi object build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
libsbi-makefile := $(lastword $(MAKEFILE_LIST))
libsbi-dir := $(patsubst %/,%,$(dir $(abspath $(libsbi-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(libsbi-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
-include $(path-cache)/deps-sbi.mk

libsbi-root := $(path-e)/libs/sbi
libsbi-obj-root := $(path-obj)/libs/sbi
libsbi-target ?= $(path-bin)/libs/$(arch)/libsbi.a

src-root := $(libsbi-root)
obj-root := $(libsbi-obj-root)
target := $(libsbi-target)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
flags-c += $($(arch)-flags-c)

includes-c += $(or $(sbi-includes-c-$(arch)),$(sbi-includes-c))
includes-cpp += $(or $(sbi-includes-cpp-$(arch)),$(sbi-includes-cpp))
includes-asm += $(or $(sbi-includes-asm-$(arch)),$(sbi-includes-asm))

include $(libsbi-root)/flags.mk
component-root := $(libsbi-root)
include $(path-s)/build/collector.mk

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
