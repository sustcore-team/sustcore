include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/libs/$(architecture)/libkmod.a

objects := startup.o

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os -g

include-c := -I$(path-include) -I$(path-include)/std/ -I$(path-include)/libs/ -I./third_party/include/ -I./third_party/include/std/

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"

args := $(args-c)
args-kernel  := $(args-c-kernel)

dir := dir-obj="$(path-objects)/kmod/$(architecture)" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-s)=$(target) architecture=$(architecture) objects="$(objects)" $(dir) $(args) $(target)