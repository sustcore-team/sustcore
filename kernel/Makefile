include $(global-env)

include $(path-script)/env/local.mk
include $(path-script)/helper.mk

.PHONY: build

target := $(path-kernel)
target-phy := $(path-kernel-phy)

objects := main.o cxa.o assert.o
deps := main.d cxa.d assert.d
attachments :=

subdirs := ./arch/$(architecture)/  ./cap  ./device ./event ./mem ./schd ./task ./vfs

include $(path-d)/arch/$(architecture)/subdirs.mk
include $(foreach subdir, $(subdirs), $(path-d)/$(subdir)/include.mk)

flags-ld := -z max-page-size=0x100 -nostdlib

# C
flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc++ \
		     -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		     -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
		     -fno-stack-protector -Wno-int-to-pointer-cast \
		     -fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medany

include-third_party-c := -I$(path-third_party)/include -I$(path-third_party)/include/libfdt -I$(path-third_party)/include/std
include-c := -I$(path-include) -I$(path-include)/std $(include-third_party-c) -I$(path-d) -I$(path-include)/arch

defs-c :=

# CPP
flags-cpp := -std=gnu++26 -nostdlib -ffreestanding -nostdinc \
		   -Wall -Wno-sign-compare -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast -fno-rtti -fno-exceptions \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medany

include-third_party-cpp := -I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
						   -I$(path-third_party)/include/std
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
			   $(include-third_party-cpp) -I$(path-d) -I$(path-include)/arch
defs-cpp :=

# ASM
flags-asm := 

ifeq ($(build-mode), debug)
	flags-c += -O0 -g
	defs-c +=
	flags-cpp += -O0 -g
	defs-cpp +=
	flags-ld +=
else
	flags-c += -Ofast
	defs-c += -DNDEBUG
	flags-cpp += -Ofast
	defs-cpp += -DNDEBUG
	flags-ld += --strip-all
endif

# Makefile args

libs := kersbi kerbasecpp fdt

args-ld := flags-ld="$(flags-ld)" script-ld="$(path-d)/arch/$(architecture)/kernel.ld"
args-phy-ld := flags-ld="$(flags-ld)" script-ld="$(path-d)/arch/$(architecture)/kernel-phy.ld"

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"
args-cpp := defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp)"
args-asm := flags-asm="$(flags-asm)"

args := $(args-ld) $(args-c) $(args-cpp) $(args-asm) libraries="$(libs)"
args-phy := $(args-phy-ld) $(args-c) $(args-cpp) $(args-asm) libraries="$(libs)"

dir := dir-obj="$(path-objects)/kernel" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-k)=$(target) architecture=$(architecture) attachments="$(attachments)" deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)
	$(q)$(MAKE) $(builder-k)=$(target-phy) architecture=$(architecture) attachments="$(attachments)" deps="$(deps)" objects="$(objects)" $(dir) $(args-phy) $(target-phy)
