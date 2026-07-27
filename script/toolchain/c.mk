# C compiler selection for freestanding targets and the validated host.
include $(path-s)/toolchain/environment.mk

# 使用的 clang 可以通过配置 clang.toml 的 (arch).clang 字段指定
riscv64-comp-c ?= clang
loongarch64-comp-c ?= clang

# riscv64 flags, 可以通过配置 clang.toml 的 flags.clang.(arch) 字段指定
riscv64-flags-c ?=
riscv64-flags-c += -target riscv64-unknown-elf

# loongarch64 flags, 可以通过配置 clang.toml 的 flags.clang.(arch) 字段指定
loongarch64-flags-c ?=
loongarch64-flags-c += -target loongarch64-linux-gnusf

# The computed variable names route exactly one candidate into y-*.
$(is-host)-toolchain-comp-c := $(host-comp-c)
$(is-freestanding)-toolchain-comp-c := $($(arch)-comp-c)
comp-c ?= $(y-toolchain-comp-c)

$(is-host)-toolchain-flags-c := $(host-sysroot-flag) $(host-cflags) \
    $(host-mode-flags) $(host-sanitize-compile-flags)
$(is-freestanding)-toolchain-flags-c := $(freestanding-config-flags-c) \
    -ffreestanding $($(arch)-flags-c)
flags-c += $(y-toolchain-flags-c)
