# Common component compilation layer. It prepares the selected build
# environment, resolves sources, and compiles objects without deciding how the
# final artifact is produced.

ifeq ($(origin global-env), undefined)
ifeq ($(strip $(component-repo-root)),)
$(error component-repo-root must be set when global-env is not provided)
endif
global-env := $(component-repo-root)/script/env/global.mk
endif

ifeq ($(origin path-s), undefined)
include $(global-env)
endif

ifeq ($(origin ctx), undefined)
$(error component $(or $(component-root),<unknown>) must be invoked with ctx)
endif

environment ?= freestanding
include $(path-s)/toolchain/environment.mk

$(is-host)-component-buildpath := host-buildpath
$(is-freestanding)-component-buildpath := buildpath
include $(path-s)/env/$(y-component-buildpath).mk

include $(ctx)
include $(path-cache)/libraries.mk
-include $(path-cache)/attachments.mk

component-root ?= $(owner-root)
component-dependency-owner ?= $(owner-id)
component-include-owner ?= $(owner-id)
component-deps-file ?= $(or $(deps-file),$(path-deps)/$(component-dependency-owner).mk)
-include $(component-deps-file)

src-root := $(owner-root)

component-use-public-includes ?= n
component-public-includes-c-y := $(library-$(component-include-owner)-include-c)
component-public-includes-cpp-y := $(library-$(component-include-owner)-include-cpp)
component-public-includes-asm-y := $(library-$(component-include-owner)-include-asm)
includes-c += $(component-public-includes-c-$(component-use-public-includes))
includes-cpp += $(component-public-includes-cpp-$(component-use-public-includes))
includes-asm += $(component-public-includes-asm-$(component-use-public-includes))

component-use-dependency-includes ?= y
component-dependency-includes-c-y := $($(component-dependency-owner)-includes-c)
component-dependency-includes-cpp-y := $($(component-dependency-owner)-includes-cpp)
component-dependency-includes-asm-y := $($(component-dependency-owner)-includes-asm)
includes-c += $(component-dependency-includes-c-$(component-use-dependency-includes))
includes-cpp += $(component-dependency-includes-cpp-$(component-use-dependency-includes))
includes-asm += $(component-dependency-includes-asm-$(component-use-dependency-includes))

component-config-mks ?=
include $(component-config-mks)
-include $(owner-root)/flags.mk

component-use-collector ?= y
component-collector-y := $(owner-root)/collect.mk
component-collector-n :=
-include $(component-collector-$(component-use-collector))

sources-asm ?= $(strip $(sources-y-asm))
sources-c ?= $(strip $(sources-y-c))
sources-cpp ?= $(strip $(sources-y-cpp))

objects-asm := $(addprefix $(obj-root)/,$(sources-asm:.S=.o))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
component-extra-objects ?=
component-attachment-objects := $($(owner-id)-attachment-objects)
objects := $(objects-asm) $(objects-c) $(objects-cpp) $(component-extra-objects) $(component-attachment-objects)
deps := $(filter %.d,$(objects:.o=.d))

component-languages := \
	$(if $(strip $(sources-asm)),asm) \
	$(if $(strip $(sources-c)),c) \
	$(if $(strip $(sources-cpp)),cpp)
component-toolchain-languages := \
	$(if $(filter asm c,$(component-languages)),c) \
	$(if $(filter cpp,$(component-languages)),cpp)

include $(foreach language,$(sort $(component-toolchain-languages)),$(path-s)/toolchain/$(language).mk)

comp-asm ?= $(comp-c)
$(is-host)-component-flags-asm := $(host-sysroot-flag) $(host-cflags) \
	$(host-mode-flags) $(host-sanitize-compile-flags)
$(is-freestanding)-component-flags-asm := $($(arch)-flags-c)
flags-asm += $(y-component-flags-asm)

$(is-host)-component-toolchain-prerequisite := $(toolchain-stamp)
$(objects): $(y-component-toolchain-prerequisite)

include $(foreach language,$(component-languages),$(path-s)/rules/$(language).mk)

component-dep-archives := $($(component-dependency-owner)-dep-archives)

.PHONY: component-vars
component-vars:
	$(q)$(echo) "component=$(owner-id)"
	$(q)$(echo) "sources=$(sources-asm) $(sources-c) $(sources-cpp)"
	$(q)$(echo) "objects=$(objects)"

-include $(deps)
