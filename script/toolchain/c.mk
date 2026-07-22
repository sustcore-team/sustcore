# 使用的 clang 可以通过配置 clang.toml 的 (arch).clang 字段指定
riscv64-comp-c ?= clang
loongarch64-comp-c ?= clang

# riscv64 flags, 可以通过配置 clang.toml 的 flags.clang.(arch) 字段指定
riscv64-flags-c ?=
riscv64-flags-c += -target riscv64-unknown-elf

# loongarch64 flags, 可以通过配置 clang.toml 的 flags.clang.(arch) 字段指定
loongarch64-flags-c ?=
loongarch64-flags-c += -target loongarch64-linux-gnusf

# common flags, 可以通过配置 clang.toml 的 flags.clang 字段指定
flags-c ?=
flags-c += -ffreestanding