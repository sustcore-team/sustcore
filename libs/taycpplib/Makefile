# Build the host implementation archive and validate the freestanding panic
# provider contract through the library's own build description.
ifeq ($(origin global-env), undefined)
taycpplib-makefile := $(lastword $(MAKEFILE_LIST))
taycpplib-dir := $(patsubst %/,%,$(dir $(abspath $(taycpplib-makefile))))
repo-root := $(patsubst %/,%,$(dir $(patsubst %/,%,$(dir $(taycpplib-dir)))))
global-env := $(repo-root)/script/env/global.mk
endif

ifeq ($(origin ctx), undefined)
$(error libs/taycpplib/Makefile must be invoked with ctx from the top-level Makefile)
endif

include $(global-env)
environment ?= freestanding
include $(path-s)/toolchain/environment.mk
$(is-host)-taycpplib-buildpath := host-buildpath
$(is-freestanding)-taycpplib-buildpath := buildpath
include $(path-s)/env/$(y-taycpplib-buildpath).mk
include $(ctx)
include $(path-cache)/libraries.mk
-include $(or $(deps-file),$(path-deps)/$(owner-id).mk)

src-root := $(owner-root)

include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ar.mk

includes-cpp += $($(owner-id)-includes-cpp)

include $(owner-root)/flags.mk
include $(owner-root)/include.mk
include $(owner-root)/collect.mk

sources-cpp := $(strip $(sources-y-cpp))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects := $(objects-cpp)
deps := $(objects:.o=.d)

$(is-host)-taycpplib-toolchain-stamp := $(toolchain-stamp)
$(objects): $(y-taycpplib-toolchain-stamp)

include $(path-s)/rules/cpp.mk
$(is-host)-taycpplib-ar-rule := $(path-s)/rules/ar.mk
-include $(y-taycpplib-ar-rule)

panic-check-root := $(obj-root)/checks/panic
panic-check-user := $(panic-check-root)/testbench/test/panic_link_user.o
panic-check-provider := $(panic-check-root)/testbench/test/panic_provider.o
panic-check-failed-output := $(panic-check-root)/must-not-link
panic-check-linked-output := $(panic-check-root)/with-provider
panic-check-link-flags := -fuse-ld=lld -nostdlib -Wl,-e,main

$(panic-check-user): $(src-root)/testbench/test/panic_link_user.cpp
	$(q)$(mkdir) $(@D)
	$(q)$(comp-cpp) -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ \
		$(flags-cpp) $(macros-cpp) $(includes-cpp) $(environment-macros-cpp) $<

$(panic-check-provider): $(src-root)/testbench/test/panic_provider.cpp
	$(q)$(mkdir) $(@D)
	$(q)$(comp-cpp) -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ \
		$(flags-cpp) $(macros-cpp) $(includes-cpp) $(environment-macros-cpp) $<

.PHONY: build-static check-freestanding taycpplib-vars
build-static: $(target)

check-freestanding: $(panic-check-user) $(panic-check-provider)
	$(if $(filter y,$(is-freestanding)),,$(error check-freestanding requires environment=freestanding))
	$(q)if $(comp-cpp) $(flags-cpp) $(panic-check-link-flags) \
		-o $(panic-check-failed-output) $(panic-check-user) >/dev/null 2>&1; then \
		echo "panic link unexpectedly succeeded without a provider" >&2; exit 1; \
	fi
	$(q)$(comp-cpp) $(flags-cpp) $(panic-check-link-flags) \
		-o $(panic-check-linked-output) $(panic-check-user) $(panic-check-provider)

taycpplib-vars:
	$(q)$(echo) "taycpplib target=$(target)"
	$(q)$(echo) "taycpplib objects=$(objects)"

-include $(deps)
-include $(panic-check-user:.o=.d) $(panic-check-provider:.o=.d)
