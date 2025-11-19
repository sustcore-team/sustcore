override flags-c += -m32 -mregparm=1 -mpreferred-stack-boundary=2 -march=i386 -mfpmath=387 -mno-mmx -mno-sse -DBITS=32
override flags-asm += --32

x86-compiler-prefix ?= x86-elf-
prefix-compiler ?= $(x86-compiler-prefix)

$(warning "ARCH=x86")