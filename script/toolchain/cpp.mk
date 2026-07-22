# 使用的 clang++ 可以通过配置 clang.toml 的 (arch).clang++ 字段指定
riscv64-comp-cpp ?= clang++
loongarch64-comp-cpp ?= clang++

# riscv64 flags, 可以通过配置 clang.toml 的 flags.clang++.(arch) 字段指定
riscv64-flags-cpp ?=
riscv64-flags-cpp += -target riscv64-unknown-elf

# loongarch64 flags, 可以通过配置 clang.toml 的 flags.clang++.(arch) 字段指定
loongarch64-flags-cpp ?=
loongarch64-flags-cpp += -target loongarch64-linux-gnusf

# common flags, 可以通过配置 clang.toml 的 flags.clang++ 字段指定
flags-cpp ?=
flags-cpp += -ffreestanding -nostdlib++