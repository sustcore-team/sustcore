include $(global-env)
include $(path-s)/env/buildpath.mk
include $(path-cache)/libraries.mk
-include $(deps-file)

include $(path-s)/toolchain/cpp.mk
includes-cpp += $(library-$(library-owner)-include-cpp)
includes-cpp += $($(library-owner)-includes-cpp)

check-root := $(path-obj)/checks/$(library-owner)/panic
user-object := $(check-root)/panic_link_user.o
provider-object := $(check-root)/panic_provider.o
failed-output := $(check-root)/must-not-link
linked-output := $(check-root)/with-provider
panic-link-flags := -fuse-ld=lld -nostdlib -Wl,-e,main

$(user-object): $(path-e)/libs/taycpplib/testbench/test/panic_link_user.cpp
	$(q)$(mkdir) $(@D)
	$(q)$(comp-cpp) -c -o $@ $(flags-cpp) $(includes-cpp) $(environment-macros-cpp) $<

$(provider-object): $(path-e)/libs/taycpplib/testbench/test/panic_provider.cpp
	$(q)$(mkdir) $(@D)
	$(q)$(comp-cpp) -c -o $@ $(flags-cpp) $(includes-cpp) $(environment-macros-cpp) $<

.PHONY: check
check: $(user-object) $(provider-object)
	$(q)if $(comp-cpp) $(flags-cpp) $(panic-link-flags) -o $(failed-output) $(user-object) >/dev/null 2>&1; then echo "panic link unexpectedly succeeded without a provider" >&2; exit 1; fi
	$(q)$(comp-cpp) $(flags-cpp) $(panic-link-flags) -o $(linked-output) $(user-object) $(provider-object)
