global-env := script/env/global.mk
include $(global-env)
include $(path-s)/target/init.mk

.PHONY: init
init: init-build-system
	$(q)$(echo) "Initialization done!"

include $(path-s)/target/switch.mk