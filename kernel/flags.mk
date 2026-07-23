# Kernel-wide compile flags and preprocessor macros.

flags-c += -ffreestanding
flags-cpp += -ffreestanding -nostdlib++ -std=c++23
flags-asm += -ffreestanding

macros-c += -D__KERNEL__
macros-cpp += -D__KERNEL__
macros-asm += -D__KERNEL__
