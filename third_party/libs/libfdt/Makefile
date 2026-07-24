# libfdt object and archive build skeleton driven by the top-level Makefile or direct make.
ifeq ($(origin global-env), undefined)
libfdt-makefile := $(lastword $(MAKEFILE_LIST))
libfdt-dir := $(patsubst %/,%,$(dir $(abspath $(libfdt-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(libfdt-dir)))))))
global-env := $(repo-root)/script/env/global.mk
endif

include $(global-env)
include $(path-s)/env/buildpath.mk

libfdt-root := $(path-e)/third_party/libs/libfdt
libfdt-obj-root := $(path-obj)/third_party/libs/libfdt
libfdt-target ?= $(path-bin)/libs/$(arch)/libfdt.a

src-root := $(libfdt-root)
obj-root := $(libfdt-obj-root)
target := $(libfdt-target)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/ar.mk

comp-c ?= $($(arch)-comp-c)
flags-c += $($(arch)-flags-c)

include $(libfdt-root)/flags.mk
include $(libfdt-root)/include.mk

sources-c := $(strip $(sources-c))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects := $(objects-c)
deps := $(objects:.o=.d)

include $(path-s)/rules/c.mk
include $(path-s)/rules/ar.mk

-include $(deps)

.PHONY: build-static libfdt-vars
build-static: $(target)

libfdt-vars:
	$(q)$(echo) "libfdt target=$(target)"
	$(q)$(echo) "libfdt objects=$(objects)"
