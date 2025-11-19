override flags-c += -mno-red-zone -m64 -DBITS=64
override flags-asm += --64

x86_64-compiler-prefix ?= x86_64-elf-
prefix-compiler ?= $(x86_64-compiler-prefix)

$(warning "ARCH=x86_64")