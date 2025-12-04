include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/libs/$(architecture)/libkmod.a

objects := startup.o rpc.o alloc.o syscall.o device.o stdio.o
deps := startup.d rpc.d alloc.d syscall.d device.d stdio.d

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os -g -mcmodel=medlow

include-c := -I$(path-include) -I$(path-include)/std/ -I$(path-include)/libs/ -I./third_party/include/ -I./third_party/include/std/  -I$(path-d)

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"

args := $(args-c)

dir := dir-obj="$(path-objects)/kmod/$(architecture)" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-s)=$(target) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)