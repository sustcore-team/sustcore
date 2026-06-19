component-kind := kernel
component-name := kernel
component-variants := default
component-active-variant := $(architecture)
component-target := $(path-kernel)
component-objdir := $(path-objects)/kernel

attachments := initrd.tar.attachment.o

variant.riscv64.target := $(path-kernel)
variant.riscv64.dir-obj := $(path-objects)/kernel
variant.riscv64.script-ld := $(component-root)/boot/sbi/sbi.ld
variant.riscv64.libraries := kersbi kerbasecpp fdt

variant.loongarch64.target := $(path-kernel)
variant.loongarch64.dir-obj := $(path-objects)/kernel
variant.loongarch64.script-ld := $(component-root)/boot/laboot/laboot.ld
variant.loongarch64.libraries := kerbasecpp fdt

variant.default.target := $(path-kernel)
variant.default.dir-obj := $(path-objects)/kernel
variant.default.script-ld := $(component-root)/boot/laboot/laboot.ld
variant.default.libraries := kerbasecpp fdt

flags-ld := $(flags-kernel-ld) $(flags-common-ld) $(flags-mode-ld)

flags-c := $(flags-common-c) -nostdinc++ $(flags-mode-c) $(kernel-flags)
include-c := -I$(path-include) -I$(path-include)/std \
	-I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
	-I$(path-third_party)/include/std -I$(path-third_party)/include/std/c++ \
	-I$(component-root) -I$(path-include)/arch
defs-c := -DASSERT_IMPLEMENTED=1 $(defs-mode-c)

features ?=
base-features := NO_RTTI NO_EXCEPTIONS
base-feature-defs := $(foreach feature, $(base-features), -D__SUS_$(feature)__=1)

-include $(component-root)/feature.mk

flags-cpp := $(flags-common-cpp) -nostdinc $(flags-no-rtti-cpp) $(flags-no-exceptions-cpp) \
	$(flags-mode-cpp) $(kernel-flags)
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
	-I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
	-I$(path-third_party)/include/std -I$(component-root) -I$(path-include)/arch
defs-cpp := -DASSERT_IMPLEMENTED=1 -DUSE_SUSTCORE_FEATURES $(base-feature-defs) $(kernel-feature-defs) $(defs-mode-cpp)

include-asm := -I$(path-include) -I$(path-include)/std \
	-I$(path-third_party)/include -I$(path-third_party)/include/std \
	-I$(component-root) -I$(path-include)/arch

# ifeq ($(architecture),loongarch64)
# flags-cpp += -mno-lsx
# endif