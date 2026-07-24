# Kernel-wide compile flags and preprocessor macros.

flags-c   += -ffreestanding
flags-cpp += -ffreestanding  -std=c++23 -nostdlib++ -fno-rtti -fno-exceptions
flags-asm += -ffreestanding

ifeq ($(arch),riscv64)
flags-c += -mcmodel=medany -fno-pic -fno-asynchronous-unwind-tables
flags-cpp += -mcmodel=medany -fno-pic -fno-asynchronous-unwind-tables
flags-asm += -mcmodel=medany -fno-pic -fno-asynchronous-unwind-tables
endif

macros-c += -D__KERNEL__
macros-cpp += -D__KERNEL__
macros-asm += -D__KERNEL__
