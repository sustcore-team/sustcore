global-env := ./script/env/global.mk
config-json := $(firstword $(wildcard config.json) script/config.default.json)
config-arch := $(shell python3 -c "import json; from pathlib import Path; p=Path('$(config-json)'); print(json.load(p.open()).get('arch','riscv64'))")

-include ./config.mk

architecture ?= $(config-arch)
config-arch-override :=
ifeq ($(origin architecture), command line)
config-arch-override := $(architecture)
endif

all: autotest

autotest: kernel-rv kernel-la

kernel-rv:
	$(q)$(MAKE) build architecture=riscv64 build-mode=$(build-mode)
	$(q)$(copy) build/$(build-mode)/riscv64/bin/kernel/sustcore.bin $@

kernel-la:
	$(q)$(MAKE) build architecture=loongarch64 build-mode=$(build-mode)
	$(q)$(copy) build/$(build-mode)/loongarch64/bin/kernel/sustcore.bin $@

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

.PHONY: build mount umount image __image stat_code all autotest kernel-rv kernel-la run dbg clean FORCE
.PHONY: build-libs build-mods build-kernel make-initrd

build-mode ?= release
kernel-flags ?=

arg-basic :=  q=$(q) build-mode=$(build-mode) architecture=$(architecture) \
	global-env=$(global-env) kernel-flags="$(kernel-flags)" features="$(features)"

-include $(path-script)/config.mk

library-components := sbi basecpp kmod linuxss-libc rpc libfdt
module-components := default init contest-runner linux-subsystem test-linux test_endpoint_master test_endpoint_slave test_call_service test_call_user \
	test_fork test_execve test_thread test_rpc_server test_rpc_client \
	test_file_rw_a test_file_rw_b test_ext4_read test_ext4_create test_ext4_rw test_ext4_symlink \
	test_fs_score test_page_cache test_page_cache_perf test_file_backed_memory test-elf-demand test-elf-demand-perf test-elf-demand-perf-child

library-component-makefile.sbi := $(path-e)/libs/sbi/Makefile
library-component-makefile.basecpp := $(path-e)/libs/basecpp/Makefile
library-component-makefile.kmod := $(path-e)/libs/kmod/Makefile
library-component-makefile.linuxss-libc := $(path-e)/libs/linuxss-libc/Makefile
library-component-makefile.rpc := $(path-e)/libs/rpc/Makefile
library-component-makefile.libfdt := $(path-e)/third_party/libs/libfdt/Makefile

module-component-makefile.default := $(path-e)/module/default/Makefile
module-component-makefile.init := $(path-e)/module/init/Makefile
module-component-makefile.contest-runner := $(path-e)/module/contest-runner/Makefile
module-component-makefile.linux-subsystem := $(path-e)/module/linux-subsystem/Makefile
module-component-makefile.test-linux := $(path-e)/module/test-linux/Makefile
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
module-component-makefile.test_ext4_read := $(path-e)/module/test_ext4_read/Makefile
module-component-makefile.test_ext4_create := $(path-e)/module/test_ext4_create/Makefile
module-component-makefile.test_ext4_rw := $(path-e)/module/test_ext4_rw/Makefile
module-component-makefile.test_ext4_symlink := $(path-e)/module/test_ext4_symlink/Makefile
module-component-makefile.test_fs_score := $(path-e)/module/test_fs_score/Makefile
module-component-makefile.test_page_cache := $(path-e)/module/test_page_cache/Makefile
module-component-makefile.test_page_cache_perf := $(path-e)/module/test_page_cache_perf/Makefile
module-component-makefile.test_file_backed_memory := $(path-e)/module/test_file_backed_memory/Makefile
module-component-makefile.test-elf-demand := $(path-e)/module/test-elf-demand/Makefile
module-component-makefile.test-elf-demand-perf := $(path-e)/module/test-elf-demand-perf/Makefile
module-component-makefile.test-elf-demand-perf-child := $(path-e)/module/test-elf-demand-perf-child/Makefile

build-libs:
ifneq ($(architecture),loongarch64)
	$(q)$(MAKE) -f $(library-component-makefile.sbi) $(arg-basic) build
endif
	$(q)$(MAKE) -f $(library-component-makefile.basecpp) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.basecpp) $(arg-basic) build-basecpp-kernel
	$(q)$(MAKE) -f $(library-component-makefile.kmod) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.linuxss-libc) $(arg-basic) build
# 	$(q)$(MAKE) -f $(library-component-makefile.rpc) $(arg-basic) build
	$(q)$(MAKE) -f $(library-component-makefile.libfdt) $(arg-basic) build
	$(q)echo "All libraries built successfully."

config.mk: FORCE $(config-json) tools/config_gen/config_gen.py
	$(q)python3 tools/config_gen/config_gen.py $(config-json) config.mk $(config-arch-override)

kernel/logger.h: FORCE $(config-json) kernel/logger.json tools/logger_gen/logger_gen.py
	$(q)python3 tools/logger_gen/logger_gen.py kernel/logger.json kernel/logger.h $(config-json) $(config-arch-override)

module/linux-subsystem/logger.h: FORCE $(config-json) module/linux-subsystem/logger.json tools/logger_gen/logger_gen.py
	$(q)python3 tools/logger_gen/logger_gen.py module/linux-subsystem/logger.json module/linux-subsystem/logger.h $(config-json) $(architecture) linuxss.logger linuxss.logger-disable-all lputer syscall.h "size_t len = 0; while (str[len] != '\0') { ++len; } sys_write_serial(0, str, len); return static_cast<int>(len);"

kernel/feature.mk: FORCE $(config-json) kernel/feature.json tools/feature_gen/feature_gen.py
	$(q)python3 tools/feature_gen/feature_gen.py kernel/feature.json kernel/feature.mk $(config-json) $(config-arch-override)

build-mods: make-initrd build-libs
	$(q)$(MAKE) -f $(module-component-makefile.default) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.init) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.contest-runner) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.linux-subsystem) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test-linux) $(arg-basic) build
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
	$(q)$(MAKE) -f $(module-component-makefile.test_ext4_read) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_ext4_create) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_ext4_rw) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_ext4_symlink) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_fs_score) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_page_cache) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_page_cache_perf) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test_file_backed_memory) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test-elf-demand) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test-elf-demand-perf) $(arg-basic) build
	$(q)$(MAKE) -f $(module-component-makefile.test-elf-demand-perf-child) $(arg-basic) build
	$(q)echo "All modules built successfully."

make-initrd:
	$(call if_mkdir, $(path-initrd))
	$(q)$(rm) -rf $(path-initrd)/tmp
	$(call if_mkdir, $(path-initrd)/tmp)
	cp -r ./tmp/$(architecture)/* $(path-initrd)/tmp/
	$(q)echo "initrd path created"

build-kernel: build-mods
	$(q)$(copy) ./LICENSE $(path-initrd)/license
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile $(arg-basic) build

build: build-kernel

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


run: build
	$(qemu-run-command)

dbg: build
	$(qemu-dbg-command)

clean:
	rm -rf $(path-e)/build kernel-rv kernel-la

build-libs build-mods make-initrd build-kernel build all autotest kernel-rv kernel-la run dbg run-only dbg-only image __image mount umount: config.mk kernel/logger.h module/linux-subsystem/logger.h kernel/feature.mk

include $(path-script)/setup.mk
