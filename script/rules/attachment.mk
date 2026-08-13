# Convert a freestanding module binary into an ELF relocatable object.

riscv64-attachment-elf-format := elf64-littleriscv
riscv64-attachment-bfd-arch := riscv
loongarch64-attachment-elf-format := elf64-loongarch
loongarch64-attachment-bfd-arch := loongarch

attachment-objcopy ?= llvm-objcopy
attachment-elf-format := $($(arch)-attachment-elf-format)
attachment-bfd-arch := $($(arch)-attachment-bfd-arch)
attachment-comma := ,
attachment-section-flags = alloc,load,$(if $(filter .rodata%,$1),readonly$(attachment-comma),)data,contents

$(obj-root)/attachment/%.attachment.o: $(path-bin)/module/%
	$(q)$(mkdir) $(@D)
	$(q)$(attachment-objcopy) -I binary -O $(attachment-elf-format) -B $(attachment-bfd-arch) \
		--rename-section \
		.data=$($(owner-id)-attachment-$*-section),$(call attachment-section-flags,$($(owner-id)-attachment-$*-section)) \
		$< $@
