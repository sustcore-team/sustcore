# Usrboot module link layer built on the common component compilation rules.
component-root := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
component-repo-root := $(abspath $(component-root)/../..)
include $(component-repo-root)/script/build/component.mk

include $(path-s)/toolchain/ld.mk
include $(path-s)/rules/attachment.mk
link-script := $(owner-root)/arch/$(arch)/usrboot.ld
include $(path-s)/rules/program-ld.mk
ld-target := $(target).elf
include $(path-s)/rules/ld.mk

.PHONY: build init-vars
build: $(target)

mk-usrboot := $(path-build-root)/$(mode)/host-tool/mk-usrboot

$(target): $(ld-target) $(mk-usrboot)
	$(q)$(call shq,$(mk-usrboot)) $(call shq,$(ld-target)) -o $(call shq,$@)

init-vars:
	$(q)$(echo) "init target=$(target)"
	$(q)$(echo) "init ld-target=$(ld-target)"
	$(q)$(echo) "init link-script=$(link-script)"
	$(q)$(echo) "init crt-head=$(crt-head)"
	$(q)$(echo) "init archives=$(archives)"
