override flags-c += -DBITS=64
override flags-asm +=

loongarch64-compiler-prefix ?= loongarch64-unknown-elf-
prefix-compiler ?= $(loongarch64-compiler-prefix)

$(warning "ARCH=loongarch64")