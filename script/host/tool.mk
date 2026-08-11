# Shared Host build-tool executable layer. The tool Makefile selects sources;
# component.mk owns the validated Host environment and object compilation.
component-use-collector := n
component-use-public-includes := n
component-dependency-owner := $(host-tool-id)
include $(path-s)/build/component.mk

include $(path-s)/toolchain/ld.mk
archives := $(component-dep-archives)

$(target): $(objects) $(archives)
	$(q)$(mkdir) $(@D)
	$(q)$(if $(strip $(sources-cpp)),$(comp-ld-cpp),$(comp-ld-c)) -o $@ $(objects) $(archives) $(flags-ld)
