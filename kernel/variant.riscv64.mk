# RISC-V64 kernel defaults.

enable-sbi ?= y
enable-laboot ?= n

macros-c += -D__ARCH_RISCV64__
macros-cpp += -D__ARCH_RISCV64__
macros-asm += -D__ARCH_RISCV64__
