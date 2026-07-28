# Init module link layer built on the common component compilation rules.
component-root := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
component-repo-root := $(abspath $(component-root)/../..)
component-use-dependency-includes := n
include $(component-repo-root)/script/build/component.mk

include $(path-s)/toolchain/ld.mk
include $(path-s)/rules/program-ld.mk
include $(path-s)/rules/ld.mk

.PHONY: build init-vars
build: $(target)

init-vars:
	$(q)$(echo) "init target=$(target)"
	$(q)$(echo) "init link-script=$(link-script)"
	$(q)$(echo) "init crt-head=$(crt-head)"
	$(q)$(echo) "init archives=$(archives)"
