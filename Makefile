global-env := script/env/global.mk
include $(global-env)
ifeq ($(host-context),1)
include $(path-s)/env/host-buildpath.mk
else
include $(path-s)/env/buildpath.mk
endif

-include $(path-cache)/libraries.mk
-include $(path-cache)/build-libs.mk
-include $(path-cache)/programs.mk
-include $(path-cache)/testbench.mk
-include $(path-cache)/.configure.mk

required-config-fragments := config.mk libraries.mk build-libs.mk programs.mk testbench.mk
missing-config-fragments = $(filter-out $(notdir $(wildcard $(addprefix $(path-cache)/,$(required-config-fragments)))),$(required-config-fragments))

.PHONY: require-configured
require-configured:
	$(if $(strip $(cached-config)),,$(error build system is not configured; run 'make configure config=<name>'))
	$(if $(strip $(missing-config-fragments)),$(error configuration cache is incomplete ($(missing-config-fragments)); rerun 'make configure config=$(cached-config)'),:)

.PHONY: require-freestanding-selection
require-freestanding-selection:
	$(if $(filter $(arch),riscv64 loongarch64),,$(error no supported freestanding architecture selected; run 'make switch arch=<arch> mode=<mode>'))
	$(if $(filter $(mode),debug release),,$(error no supported build mode selected; run 'make switch arch=<arch> mode=<mode>'))

build-libs build-modules build-initrd build-kernel runonly dbgonly update: require-configured require-freestanding-selection
validate-host build-host-libs build-host-lib host-test example host-example bench host-bench: require-configured
host-header-check: require-configured
freestanding-header-check freestanding-check check-lib build-lib-matrix: require-configured require-freestanding-selection

kernel-path ?= $(path-bin)/kernel/sustcore.bin

include $(path-s)/target/init.mk
include $(path-s)/target/configure.mk
include $(path-s)/target/run.mk
include $(path-s)/target/clean.mk
include $(path-s)/target/initrd.mk
include $(path-s)/target/update.mk
include $(path-s)/target/host.mk

.PHONY: init
init: init-build-system
	$(q)$(echo) "Initialization done!"

.PHONY: build-kernel
build-kernel: build-initrd
	$(q)$(MAKE) -f $(path-e)/kernel/Makefile \
		global-env=$(global-env) \
		arch=$(arch) \
		q=$(q) \
		ctx=$(path-ctx)/kernel.mk \
		kernel-path=$(kernel-path) \
		build

.PHONY: run
run: build-kernel runonly

.PHONY: dbg
dbg: build-kernel dbgonly


include $(path-s)/target/switch.mk
