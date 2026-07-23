# LoongArch64 kernel defaults.

enable-sbi ?= n
enable-laboot ?= y

macros-c += -D__ARCH_LOONGARCH64__
macros-cpp += -D__ARCH_LOONGARCH64__
macros-asm += -D__ARCH_LOONGARCH64__
