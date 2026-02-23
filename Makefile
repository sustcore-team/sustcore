global-env := ./script/env/global.mk

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

.PHONY: build mount umount image __image stat_code all dbg clean
.PHONY: build-libsbi build-libbasecpp build-libkmod build-libfdt build-libs

path-bin := $(path-e)/build/bin
path-objects := $(path-e)/build/objects
build-mode ?= release

arg-basic := build-mode=$(build-mode) architecture=$(architecture) global-env=$(global-env) path-bin=$(path-bin) path-objects=$(path-objects) q=$(q)

-include $(path-script)/config.mk

build-libsbi:
	$(q)$(MAKE) -f $(path-e)/libs/sbi/Makefile $(arg-basic) build

build-libbasecpp:
	$(q)$(MAKE) -f $(path-e)/libs/basecpp/Makefile $(arg-basic) build

build-libkmod:
	$(q)$(MAKE) -f $(path-e)/libs/kmod/Makefile $(arg-basic) build

build-libfdt:
	$(q)$(MAKE) -f $(path-e)/third_party/libs/libfdt/Makefile $(arg-basic) build

build-libs: build-libsbi build-libbasecpp build-libkmod build-libfdt
	$(q)echo "All libraries built successfully."

build-moddefault: build-libs
	$(q)$(MAKE) -f $(path-e)/module/default/Makefile $(arg-basic) build

build-mods: build-moddefault
	$(q)echo "All modules built successfully."

make-initrd:
	$(call if_mkdir, $(path-initrd))
	$(q)echo "initrd path created"

build: make-initrd build-mods
	$(q)$(copy) ./LICENSE $(path-initrd)/license
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile $(arg-basic) $@

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

	$(q)$(MAKE) $(imager)=$(path-e)/build/bin/kernel/sustcore.bin path=/Sustcore/System/sustcore.bin do-image

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