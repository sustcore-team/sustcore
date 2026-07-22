# 使用的 clang++ 可以通过配置 toolchain.toml 的 (arch).clang++ 字段指定
riscv-comp-cpp ?= clang++
loongarch64-comp-cpp ?= clang++

# riscv flags
riscv-flags-cpp ?=
riscv-flags-cpp += -target riscv64-unknown-elf

# loongarch flags
loongarch64-flags-cpp ?=
loongarch64-flags-cpp += -target loongarch64-linux-gnusf

# common flags
flags-cpp ?=
flags-cpp += -ffreestanding -nostdlib++