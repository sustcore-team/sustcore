# C++ compiler selection for freestanding targets and the validated host.
include $(path-s)/toolchain/environment.mk

# 使用的 clang++ 可以通过配置 clang.toml 的 (arch).clang++ 字段指定
riscv64-comp-cpp ?= clang++
loongarch64-comp-cpp ?= clang++

# riscv64 flags, 可以通过配置 clang.toml 的 flags.clang++.(arch) 字段指定
riscv64-flags-cpp ?=
riscv64-flags-cpp += -target riscv64-unknown-elf

# loongarch64 flags, 可以通过配置 clang.toml 的 flags.clang++.(arch) 字段指定
loongarch64-flags-cpp ?=
loongarch64-flags-cpp += -target loongarch64-linux-gnusf

$(is-host)-toolchain-comp-cpp := $(host-comp-cpp)
$(is-freestanding)-toolchain-comp-cpp := $($(arch)-comp-cpp)
comp-cpp ?= $(y-toolchain-comp-cpp)

$(is-host)-toolchain-flags-cpp := $(host-sysroot-flag) $(host-cxxflags) \
    $(host-feature-cxxflags) $(host-mode-flags) $(host-sanitize-compile-flags)
$(is-freestanding)-toolchain-flags-cpp := $(freestanding-config-flags-cpp) \
    -std=c++23 -ffreestanding -nostdlib++ $($(arch)-flags-cpp)
flags-cpp += $(y-toolchain-flags-cpp)
