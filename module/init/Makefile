# Minimal init module build entry.
ifeq ($(origin global-env), undefined)
init-makefile := $(lastword $(MAKEFILE_LIST))
init-dir := $(patsubst %/,%,$(dir $(abspath $(init-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(init-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin build-header), undefined)
$(error module/init/Makefile must be invoked with build-header from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
include $(build-header)
-include $(path-cache)/deps-$(owner-id).mk

src-root := $(owner-root)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ld.mk

comp-c ?= $($(arch)-comp-c)
comp-cpp ?= $($(arch)-comp-cpp)
comp-asm ?= $(comp-c)
comp-ld ?= $($(arch)-comp-ld)

flags-c += $($(arch)-flags-c)
flags-cpp += $($(arch)-flags-cpp)
flags-asm += $($(arch)-flags-c)

include $(owner-root)/flags.mk
include $(owner-root)/collect.mk

include $(path-s)/rules/program-ld.mk

sources-cpp := $(strip $(sources-y-cpp))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects := $(objects-cpp)
deps := $(objects:.o=.d)

include $(path-s)/rules/cpp.mk
include $(path-s)/rules/ld.mk

-include $(deps)

.PHONY: build init-vars
build: $(target)

init-vars:
	$(q)$(echo) "init target=$(target)"
	$(q)$(echo) "init link-script=$(link-script)"
	$(q)$(echo) "init crt-head=$(crt-head)"
	$(q)$(echo) "init archives=$(archives)"
