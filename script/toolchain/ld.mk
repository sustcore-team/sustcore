# Use the configured linker for each architecture.
riscv64-comp-ld ?= ld.lld
loongarch64-comp-ld ?= ld.lld

# Common linker flags.
flags-ld ?=
