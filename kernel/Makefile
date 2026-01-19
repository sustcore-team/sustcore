include $(global-env)

include $(path-script)/env/local.mk
include $(path-script)/helper.mk

.PHONY: build

target := $(path-kernel)
target-phy := $(path-kernel-phy)

objects := main.o
deps := main.d
attachments := license.attachment.o

subdirs := ./arch/$(architecture)/ ./mem/

include $(path-d)/arch/$(architecture)/subdirs.mk
include $(foreach subdir, $(subdirs), $(path-d)/$(subdir)/include.mk)

flags-ld := -z max-page-size=0x100 -nostdlib

args-ld := flags-ld="$(flags-ld)" script-ld="$(path-d)/arch/$(architecture)/kernel.ld"
args-phy-ld := flags-ld="$(flags-ld)" script-ld="$(path-d)/arch/$(architecture)/kernel-phy.ld"

# C
flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc++ \
		     -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		     -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
		     -fno-stack-protector -Wno-int-to-pointer-cast \
		     -fno-toplevel-reorder -fno-tree-scev-cprop -Os

flags-c += -g -mcmodel=medany

include-third_party-c := -I$(path-third_party)/include/ -I$(path-third_party)/include/libfdt/ -I$(path-third_party)/include/std/
include-c := -I$(path-include) -I$(path-include)/std/ $(include-third_party-c) -I$(path-d) -I$(path-include)/arch/

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"

# CPP

flags-cpp := -std=gnu++26 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast -fno-rtti -fno-exceptions \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os
flags-cpp += -g -mcmodel=medany
include-third_party-cpp := -I$(path-third_party)/include/ -I$(path-third_party)/include/libfdt/ \
						   -I$(path-third_party)/include/std/
include-cpp := -I$(path-include) -I$(path-include)/std/ -I$(path-include)/std/c++ \
			   $(include-third_party-cpp) -I$(path-d) -I$(path-include)/arch/
defs-cpp :=
args-cpp := defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp)"

#ASM

flags-asm := 

args-asm := flags-asm="$(flags-asm)"

libs := kersbi kerbasecpp fdt

args := $(args-ld) $(args-c) $(args-cpp) $(args-asm) libraries="$(libs)"
args-phy := $(args-phy-ld) $(args-c) $(args-cpp) $(args-asm) libraries="$(libs)"

dir := dir-obj="$(path-objects)/kernel/" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-k)=$(target) architecture=$(architecture) attachments="$(attachments)" deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)
	$(q)$(MAKE) $(builder-k)=$(target-phy) architecture=$(architecture) attachments="$(attachments)" deps="$(deps)" objects="$(objects)" $(dir) $(args-phy) $(target-phy)