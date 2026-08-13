# compile flags and preprocessor macros.

flags-c   += -ffreestanding
flags-cpp += -ffreestanding  -std=c++23 -nostdlib++ -fno-rtti -fno-exceptions
flags-asm += -ffreestanding
flags-ld  += -z norelro

ifeq ($(arch),riscv64)
flags-c += -fno-pic -fno-asynchronous-unwind-tables
flags-cpp += -fno-pic -fno-asynchronous-unwind-tables
flags-asm += -fno-pic -fno-asynchronous-unwind-tables
endif

macros-c +=
macros-cpp += $(if $(filter riscv64,$(arch)),-D__ARCH_RISCV64__)
macros-cpp += $(if $(filter loongarch64,$(arch)),-D__ARCH_LOONGARCH64__)
macros-asm +=
