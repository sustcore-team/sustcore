global-env := ./script/env/global.mk
config-json := $(firstword $(wildcard config.json) script/config.default.json)
config-arch := $(shell python3 -c "import json; from pathlib import Path; p=Path('$(config-json)'); print(json.load(p.open()).get('arch','riscv64'))")

-include ./config.mk

architecture ?= $(config-arch)
config-arch-override :=
ifeq ($(origin architecture), command line)
config-arch-override := $(architecture)
endif

all:
	$(q)$(MAKE) -s build && $(MAKE) run

image-sectors ?= 262144

mode-boot ?= bios

filesystem ?= ext4
os-name ?= Sustcore

include $(global-env)
include $(path-script)/env/local.mk
include $(path-script)/helper.mk
include $(path-script)/tool.mk
include $(path-script)/util.mk
include $(path-script)/run.mk

.PHONY: build mount umount image __image stat_code all dbg clean FORCE
.PHONY: build-libs build-mods build-kernel make-initrd

build-mode ?= release
kernel-flags ?=

arg-basic :=  q=$(q) build-mode=$(build-mode) architecture=$(architecture) \
	global-env=$(global-env) kernel-flags="$(kernel-flags)" features="$(features)"

-include $(path-script)/config.mk

library-components := sbi basecpp kmod rpc libfdt
module-components := default init test_endpoint_master test_endpoint_slave test_call_service test_call_user \
	test_fork test_execve test_thread test_rpc_server test_rpc_client \
	test_file_rw_a test_file_rw_b

library-component-makefile.sbi := $(path-e)/libs/sbi/Makefile
library-component-makefile.basecpp := $(path-e)/libs/basecpp/Makefile
library-component-makefile.kmod := $(path-e)/libs/kmod/Makefile
library-component-makefile.rpc := $(path-e)/libs/rpc/Makefile
library-component-makefile.libfdt := $(path-e)/third_party/libs/libfdt/Makefile

module-component-makefile.default := $(path-e)/module/default/Makefile
module-component-makefile.init := $(path-e)/module/init/Makefile
module-component-makefile.test_endpoint_master := $(path-e)/module/test_endpoint_master/Makefile
module-component-makefile.test_endpoint_slave := $(path-e)/module/test_endpoint_slave/Makefile
module-component-makefile.test_call_service := $(path-e)/module/test_call_service/Makefile
module-component-makefile.test_call_user := $(path-e)/module/test_call_user/Makefile
module-component-makefile.test_fork := $(path-e)/module/test_fork/Makefile
module-component-makefile.test_execve := $(path-e)/module/test_execve/Makefile
module-component-makefile.test_thread := $(path-e)/module/test_thread/Makefile
module-component-makefile.test_rpc_server := $(path-e)/module/test_rpc_server/Makefile
module-component-makefile.test_rpc_client := $(path-e)/module/test_rpc_client/Makefile
module-component-makefile.test_file_rw_a := $(path-e)/module/test_file_rw_a/Makefile
module-component-makefile.test_file_rw_b := $(path-e)/module/test_file_rw_b/Makefile

build-libs:
ifneq ($(architecture),loongarch64)
	$(q)$(MAKE) -f $(library-component-makefile.sbi) $(arg-basic) build
endif
	$(q)$(MAKE) -f $(library-component-makefile.basecpp) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.basecpp) $(arg-basic) build-basecpp-kernel
	$(q)$(MAKE) -f $(library-component-makefile.kmod) $(arg-basic) build
# 	$(q)$(MAKE) -f $(library-component-makefile.rpc) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.libfdt) $(arg-basic) build
	$(q)echo "All libraries built successfully."

config.mk: FORCE $(config-json) tools/config_gen/config_gen.py
	$(q)python3 tools/config_gen/config_gen.py $(config-json) config.mk $(config-arch-override)

kernel/logger.h: FORCE $(config-json) kernel/logger.json tools/logger_gen/logger_gen.py
	$(q)python3 tools/logger_gen/logger_gen.py kernel/logger.json kernel/logger.h $(config-json) $(config-arch-override)

kernel/feature.mk: FORCE $(config-json) kernel/feature.json tools/feature_gen/feature_gen.py
	$(q)python3 tools/feature_gen/feature_gen.py kernel/feature.json kernel/feature.mk $(config-json) $(config-arch-override)

build-mods: build-libs
	$(q)$(MAKE) -f $(module-component-makefile.default) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.init) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_endpoint_master) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_endpoint_slave) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_call_service) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_call_user) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_fork) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_execve) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_thread) $(arg-basic) build
# 	$(q)$(MAKE) -f $(module-component-makefile.test_rpc_server) $(arg-basic) build
# 	$(q)$(MAKE) -f $(module-component-makefile.test_rpc_client) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_file_rw_a) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_file_rw_b) $(arg-basic) build
	$(q)echo "All modules built successfully."

make-initrd:
	$(call if_mkdir, $(path-initrd))
	$(q)$(rm) -rf $(path-initrd)/src
	$(call if_mkdir, $(path-initrd)/src)
	cp -r ./include/ $(path-initrd)/src/include/
	cp -r ./kernel/ $(path-initrd)/src/kernel/
	cp -r ./libs/ $(path-initrd)/src/libs/
	cp -r ./module/ $(path-initrd)/src/module/
	cp -r ./script/ $(path-initrd)/src/script/
	cp -r ./tools/ $(path-initrd)/src/tools/
	$(q)echo "initrd path created"

build-kernel:
	$(q)$(copy) ./LICENSE $(path-initrd)/license
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile $(arg-basic) build

build: make-initrd build-mods build-kernel

mount:
	$(q)$(MAKE) -f $(path-script)/image/Makefile.image global-env=$(global-env) loop=$(loop-b) start-image

umount:
	$(q)$(umount) $(path-mount)

image:
	$(q)$(MAKE) -f $(path-script)/image/Makefile.image global-env=$(global-env) loop=$(loop-b) start-image
	-$(q)$(MAKE) __image
	$(q)$(MAKE) -f $(path-script)/image/Makefile.image global-env=$(global-env) loop=$(loop-b) end-image

__image:
# 	$(q)$(MAKE) $(imager)=$(path-e)/configs/grub.cfg path=/boot/grub/ do-image
# 	$(q)$(MAKE) $(imager)=$(path-e)/build/bin/grub/grubld.bin path=/ do-image

	$(q)$(MAKE) $(imager)=$(path-kernel) path=/Sustcore/System/sustcore.bin do-image

__load_image:
	$(q)$(MAKE) -f $(path-script)/image/Makefile.image global-env=$(global-env) loop=$(loop-b) start-image

__unload_image:
	$(q)$(MAKE) -f $(path-script)/image/Makefile.image global-env=$(global-env) loop=$(loop-b) end-image

stat_code:
	$(q)$(comments-stat)

dbg:
	$(q)$(MAKE) -s build && $(MAKE) run_dbg

clean:
	rm -rf $(path-e)/build

build-libs build-mods make-initrd build-kernel build all dbg run run_dbg image __image mount umount: config.mk kernel/logger.h kernel/feature.mk

include $(path-script)/setup.mk
