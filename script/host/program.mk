# Shared host executable build rules. The library testbench Makefile selects
# sources before including this fragment.
include $(path-s)/env/host-buildpath.mk
include $(ctx)
include $(path-cache)/libraries.mk
-include $(deps-file)

src-root := $(owner-root)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ld.mk

includes-c += $(library-$(library-owner)-include-c) $($(library-owner)-includes-c)
includes-cpp += $(library-$(library-owner)-include-cpp) $($(library-owner)-includes-cpp)

objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects := $(objects-c) $(objects-cpp)
deps := $(objects:.o=.d)
owner-archive := $(if $(filter n,$(library-$(library-owner)-is-header-only)),$(path-bin)/libs/$(library-$(library-owner)-libname))
archives := $($(library-owner)-dep-archives) $(owner-archive)

$(objects): $(toolchain-stamp)

include $(path-s)/rules/c.mk
include $(path-s)/rules/cpp.mk

$(target): $(objects) $(archives)
	$(q)$(mkdir) $(@D)
	$(q)$(if $(strip $(sources-cpp)),$(comp-ld-cpp),$(comp-ld-c)) -o $@ $(objects) $(archives) $(flags-ld)

-include $(deps)
