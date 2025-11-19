override flags-c += -DBITS=64
override flags-asm +=

riscv64-compiler-prefix ?= riscv64-unknown-elf-
prefix-compiler ?= $(riscv64-compiler-prefix)

$(warning "ARCH=riscv64")