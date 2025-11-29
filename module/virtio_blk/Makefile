include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/mods/$(architecture)/virtio_blk.mod

objects := main.o
deps := main.d

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os -g

include-c := -I$(path-include) -I$(path-include)/std/ -I$(path-include)/libs/ -I./third_party/include/ -I./third_party/include/std/  -I$(path-d)

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"

flags-ld := -z max-page-size=0x100 -nostdlib

args-ld := flags-ld="$(flags-ld)" script-ld="$(path-d)/virtio_blk.ld"

args := $(args-c) $(args-ld) libraries="kerbasec kmod"

dir := dir-obj="$(path-objects)/kmod/$(architecture)" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-m)=$(target) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)