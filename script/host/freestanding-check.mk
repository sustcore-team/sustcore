# Generic freestanding compile/link contract check. Checks build target objects
# but never execute them on the build host.
include $(global-env)
environment ?= freestanding
include $(path-s)/env/buildpath.mk
include $(path-cache)/libraries.mk
-include $(deps-file)

include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk

-include $(library-root)/flags.mk

includes-c += $(library-$(library-owner)-include-c) $($(library-owner)-includes-c)
includes-cpp += $(library-$(library-owner)-include-cpp) $($(library-owner)-includes-cpp)

src-root := $(check-source-root)
obj-root := $(path-obj)/checks/freestanding/$(library-owner)/$(check-id)
sources-c := $(if $(filter c,$(check-language)),$(check-sources))
sources-cpp := $(if $(filter c++,$(check-language)),$(check-sources))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects := $(objects-c) $(objects-cpp)
deps := $(objects:.o=.d)

include $(path-s)/rules/c.mk
include $(path-s)/rules/cpp.mk

check-compiler-c := $(comp-c)
check-compiler-c++ := $(comp-cpp)
check-flags-c := $(flags-c)
check-flags-c++ := $(flags-cpp)
check-macros-c := $(macros-c)
check-macros-c++ := $(macros-cpp)
check-includes-c := $(includes-c)
check-includes-c++ := $(includes-cpp)
check-environment-c :=
check-environment-c++ := $(environment-macros-cpp)
check-compiler := $(check-compiler-$(check-language))
check-flags := $(check-flags-$(check-language))
check-macros := $(check-macros-$(check-language))
check-includes := $(check-includes-$(check-language))
check-environment := $(check-environment-$(check-language))
check-source-paths := $(addprefix $(src-root)/,$(check-sources))
check-output := $(obj-root)/linked
check-link-flags := -fuse-ld=lld -nostdlib -Wl,-e,main

.PHONY: check check-compile-success check-compile-failure
.PHONY: check-link-success check-link-failure

check-target-compile-success := check-compile-success
check-target-compile-failure := check-compile-failure
check-target-link-success := check-link-success
check-target-link-failure := check-link-failure
check-target := $(check-target-$(check-kind)-$(check-expect))

check: $(check-target)

check-compile-success: $(objects)

check-compile-failure:
	$(q)set -e; for source in $(check-source-paths); do \
		if $(check-compiler) $(check-flags) $(check-macros) $(check-includes) \
			$(check-environment) -fsyntax-only $$source >/dev/null 2>&1; then \
			echo "freestanding check $(check-id) unexpectedly compiled: $$source" >&2; \
			exit 1; \
		fi; \
	done

check-link-success: $(check-output)

$(check-output): $(objects)
	$(q)$(mkdir) $(@D)
	$(q)$(check-compiler) $(check-flags) $(check-link-flags) -o $@ $(objects)

check-link-failure: $(objects)
	$(q)if $(check-compiler) $(check-flags) $(check-link-flags) \
		-o $(check-output) $(objects) >/dev/null 2>&1; then \
		echo "freestanding check $(check-id) unexpectedly linked" >&2; \
		exit 1; \
	fi

-include $(deps)
