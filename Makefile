global-env := script/env/global.mk
include $(global-env)
include $(path-s)/env/buildpath.mk

-include $(path-cache)/libraries.mk
-include $(path-cache)/build-libs.mk
-include $(path-cache)/programs.mk

kernel-path ?= $(path-bin)/kernel/sustcore.bin

include $(path-s)/target/init.mk
include $(path-s)/target/configure.mk
include $(path-s)/target/run.mk
include $(path-s)/target/clean.mk
include $(path-s)/target/initrd.mk

.PHONY: init
init: init-build-system
	$(q)$(echo) "Initialization done!"

.PHONY: build-kernel
build-kernel: build-initrd
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile \
		global-env=$(global-env) \
		arch=$(arch) \
		q=$(q) \
		build-header=$(path-cache)/build-header-kernel.mk \
		kernel-path=$(kernel-path) \
		build

include $(path-s)/target/switch.mk
