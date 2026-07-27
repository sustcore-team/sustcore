# Linker selection for freestanding targets and compiler-driver host links.
include $(path-s)/toolchain/environment.mk

riscv64-comp-ld ?= ld.lld
loongarch64-comp-ld ?= ld.lld

$(is-freestanding)-toolchain-comp-ld := $($(arch)-comp-ld)
comp-ld ?= $(y-toolchain-comp-ld)

$(is-host)-toolchain-comp-ld-c := $(host-comp-c)
$(is-host)-toolchain-comp-ld-cpp := $(host-comp-cpp)
comp-ld-c ?= $(y-toolchain-comp-ld-c)
comp-ld-cpp ?= $(y-toolchain-comp-ld-cpp)

$(is-host)-toolchain-flags-ld := $(host-sysroot-flag) $(host-ldflags) \
    $(host-sanitize-link-flags)
$(is-freestanding)-toolchain-flags-ld :=
flags-ld += $(y-toolchain-flags-ld)
