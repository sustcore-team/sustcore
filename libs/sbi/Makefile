include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/libs/$(architecture)/libkersbi.a

objects := sbi.o sbi_dbcn.o sbi_legacy.o sbi_base.o
deps := sbi.d sbi_dbcn.d sbi_legacy.d sbi_base.d

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os -g -mcmodel=medany

include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs -I./third_party/include -I./third_party/include/std

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"

args := $(args-c)

dir := dir-obj="$(path-objects)/sbi/$(architecture)" dir-src="$(path-d)"

build:
ifeq ($(architecture), riscv64)
	$(q)$(MAKE) $(builder-s)=$(target) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)
else
	echo "!"
endif