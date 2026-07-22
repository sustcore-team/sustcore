# 使用的 clang 可以通过配置 toolchain.toml 的 (arch).clang 字段指定
riscv-comp-c ?= clang
loongarch64-comp-c ?= clang

# riscv flags
riscv-flags-c ?=
riscv-flags-c += -target riscv64-unknown-elf

# loongarch flags
loongarch64-flags-c ?=
loongarch64-flags-c += -target loongarch64-linux-gnusf

# common flags
flags-c ?=
flags-c += -ffreestanding