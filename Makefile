global-env := ./script/env/global.mk
config-json := $(firstword $(wildcard config.json) script/config.default.json)

-include ./config.mk

image-sectors ?= 262144

architecture ?= riscv64
mode-boot ?= bios

filesystem ?= ext4
os-name ?= Sustcore

include $(global-env)
include $(path-script)/env/local.mk
include $(path-script)/helper.mk
include $(path-script)/tool.mk
include $(path-script)/util.mk
include $(path-script)/setup.mk
include $(path-script)/run.mk

.PHONY: build mount umount image __image stat_code all dbg clean FORCE
.PHONY: build-libs build-mods build-kernel make-initrd

build-mode ?= release
kernel-flags ?=

arg-basic :=  q=$(q) build-mode=$(build-mode) architecture=$(architecture) \
	global-env=$(global-env) kernel-flags="$(kernel-flags)" features="$(features)"

-include $(path-script)/config.mk

library-components := sbi basecpp kmod libfdt
module-components := default init test1 test2 test_fork test_execve test_thread

library-component-makefile.sbi := $(path-e)/libs/sbi/Makefile
library-component-makefile.basecpp := $(path-e)/libs/basecpp/Makefile
library-component-makefile.kmod := $(path-e)/libs/kmod/Makefile
library-component-makefile.libfdt := $(path-e)/third_party/libs/libfdt/Makefile

module-component-makefile.default := $(path-e)/module/default/Makefile
module-component-makefile.init := $(path-e)/module/init/Makefile
module-component-makefile.test1 := $(path-e)/module/test1/Makefile
module-component-makefile.test2 := $(path-e)/module/test2/Makefile
module-component-makefile.test_fork := $(path-e)/module/test_fork/Makefile
module-component-makefile.test_execve := $(path-e)/module/test_execve/Makefile
module-component-makefile.test_thread := $(path-e)/module/test_thread/Makefile

build-libs:
	$(q)$(MAKE) -f $(library-component-makefile.sbi) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.basecpp) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.kmod) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.libfdt) $(arg-basic) build
	$(q)echo "All libraries built successfully."

config.mk: FORCE $(config-json) tools/config_gen/config_gen.py
	$(q)python3 tools/config_gen/config_gen.py $(config-json) config.mk

kernel/logger.h: FORCE $(config-json) kernel/logger.json tools/logger_gen/logger_gen.py
	$(q)python3 tools/logger_gen/logger_gen.py kernel/logger.json kernel/logger.h $(config-json)

kernel/feature.mk: FORCE $(config-json) kernel/feature.json tools/feature_gen/feature_gen.py
	$(q)python3 tools/feature_gen/feature_gen.py kernel/feature.json kernel/feature.mk $(config-json)

build-mods: build-libs
	$(q)$(MAKE) -f $(module-component-makefile.default) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.init) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test1) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test2) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_fork) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_execve) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_thread) $(arg-basic) build
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

all:
	$(q)$(MAKE) -s build && $(MAKE) run

dbg:
	$(q)$(MAKE) -s build && $(MAKE) run_dbg

clean:
	rm -rf $(path-e)/build

build-libs build-mods make-initrd build-kernel build all dbg run run_dbg image __image mount umount: config.mk kernel/logger.h kernel/feature.mk
