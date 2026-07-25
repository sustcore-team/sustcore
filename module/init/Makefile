# Minimal init module build entry.
ifeq ($(origin global-env), undefined)
init-makefile := $(lastword $(MAKEFILE_LIST))
init-dir := $(patsubst %/,%,$(dir $(abspath $(init-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(init-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
-include $(path-cache)/deps-init.mk

init-root := $(path-e)/module/init
init-obj-root := $(path-obj)/module/init
init-target ?= $(path-bin)/module/$(or $(init-output),init.mod)

program-id := init
src-root := $(init-root)
obj-root := $(init-obj-root)
target := $(init-target)

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

include $(path-s)/rules/program-ld.mk

sources-cpp := main.cpp
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
