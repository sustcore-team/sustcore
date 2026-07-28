# Shared Host testbench executable layer. Its Makefile selects sources while
# component.mk owns environment setup and object compilation.
component-use-collector := n
component-use-public-includes := y
component-dependency-owner := $(library-owner)
component-include-owner := $(library-owner)
include $(path-s)/build/component.mk

include $(path-s)/toolchain/ld.mk
owner-archive := $(if $(filter n,$(library-$(library-owner)-is-header-only)),$(path-bin)/libs/$(library-$(library-owner)-libname))
archives := $(component-dep-archives) $(owner-archive)

$(target): $(objects) $(archives)
	$(q)$(mkdir) $(@D)
	$(q)$(if $(strip $(sources-cpp)),$(comp-ld-cpp),$(comp-ld-c)) -o $@ $(objects) $(archives) $(flags-ld)
