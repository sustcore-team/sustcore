override flags-c += -DBITS=64
override flags-asm +=

loongarch64-compiler-prefix ?= loongarch64-unknown-elf-
prefix-compiler ?= $(loongarch64-compiler-prefix)

define make-attachment
	$(q)$(prefix-compiler)objcopy -I binary -O loongarch64-unknown-elf -B loongarch --rename-section .data=.attach.$(basename $(notdir $(1))) $(1) $(2)
endef

$(warning "ARCH=loongarch64")