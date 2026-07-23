global-env := script/env/global.mk
include $(global-env)
include $(path-s)/env/buildpath.mk
include $(path-s)/target/init.mk
include $(path-s)/target/configure.mk

.PHONY: init
init: init-build-system
	$(q)$(echo) "Initialization done!"

.PHONY: build-kernel
build-kernel:
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile \
		global-env=$(global-env) \
		arch=$(arch) \
		q=$(q) \
		build

include $(path-s)/target/switch.mk
