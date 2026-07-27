include $(global-env)
environment ?= freestanding
include $(path-s)/toolchain/environment.mk
$(is-host)-header-check-buildpath := host-buildpath
$(is-freestanding)-header-check-buildpath := buildpath
include $(path-s)/env/$(y-header-check-buildpath).mk
include $(path-cache)/libraries.mk
-include $(deps-file)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk

includes-c += $(library-$(library-owner)-include-c) $($(library-owner)-includes-c)
includes-cpp += $(library-$(library-owner)-include-cpp) $($(library-owner)-includes-cpp)

.PHONY: check
check:
ifeq ($(check-language),c)
	$(q){ $(foreach header,$(check-before),$(echo) '#include <$(header)>'; ) $(echo) '#include <$(check-header)>'; $(foreach header,$(check-after),$(echo) '#include <$(header)>'; ) } | $(comp-c) -x c -fsyntax-only $(flags-c) $(includes-c) -
else ifeq ($(check-language),c++)
	$(q){ $(foreach header,$(check-before),$(echo) '#include <$(header)>'; ) $(echo) '#include <$(check-header)>'; $(foreach header,$(check-after),$(echo) '#include <$(header)>'; ) } | $(comp-cpp) -x c++ -fsyntax-only $(flags-cpp) $(includes-cpp) $(environment-macros-cpp) -
else
	$(error unsupported header-check language: $(check-language))
endif
