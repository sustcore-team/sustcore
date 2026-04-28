override flags-c += -DBITS=64
override flags-asm +=

riscv64-compiler-prefix ?= riscv64-unknown-elf-
prefix-compiler ?= $(riscv64-compiler-prefix)

define make-attachment
	$(q)$(prefix-compiler)objcopy -I binary -O riscv64-unknown-elf -B riscv --rename-section .data=.attach.$(basename $(notdir $(1))) $(1) $(2)
endef

$(warning "ARCH=riscv64")