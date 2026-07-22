# 使用的 qemu 可以通过配置 qemu.toml 的 (arch).qemu 字段指定
riscv64-qemu ?= qemu-system-riscv64
loongarch64-qemu ?= qemu-system-loongarch64

riscv64-qemu-generated-args ?= -name "QEMU VM RISCV64" -m size=256m,maxmem=256m
riscv64-qemu-attached-args ?=
loongarch64-qemu-generated-args ?= -name "QEMU VM LOONGARCH64" -m size=256m,maxmem=256m
loongarch64-qemu-attached-args ?=

qemu-generated-args := $($(arch)-qemu-generated-args)
qemu-attached-args := $($(arch)-qemu-attached-args)
qemu-args := $(qemu-generated-args) $(qemu-attached-args)
