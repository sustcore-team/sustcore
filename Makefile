global-env := script/env/global.mk
include $(global-env)
include $(path-s)/env/buildpath.mk

-include $(path-cache)/libraries.mk
-include $(path-cache)/deps-kernel.mk

kernel-path ?= $(path-bin)/kernel/sustcore.bin

include $(path-s)/target/init.mk
include $(path-s)/target/configure.mk
include $(path-s)/target/run.mk

.PHONY: init
init: init-build-system
	$(q)$(echo) "Initialization done!"

.PHONY: build-kernel
build-kernel: build-libs
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile \
		global-env=$(global-env) \
		arch=$(arch) \
		q=$(q) \
		kernel-path=$(kernel-path) \
		build

.PHONY: build-init
build-init: build-libs
	$(q)$(MAKE) -f $(path-e)/module/init/Makefile \
		global-env=$(global-env) \
		arch=$(arch) \
		q=$(q) \
		build

include $(path-s)/target/switch.mk
