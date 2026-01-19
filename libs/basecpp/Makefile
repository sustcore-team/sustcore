include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/libs/$(architecture)/libbasecpp.a
target-kernel := $(path-bin)/libs/$(architecture)/libkerbasecpp.a

objects := logger.o baseio.o tostring.o string.o ctype.o stdlib.o
deps := logger.d baseio.d tostring.d string.d ctype.d stdlib.d

flags-c-kernel := -g -mcmodel=medany
flags-c-non-kernel := -g -mcmodel=medlow

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os

include-c := -I$(path-include) -I$(path-include)/std/ -I$(path-include)/libs/ -I./third_party/include/ -I./third_party/include/std/

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c) $(flags-c-non-kernel)"
args-c-kernel := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c) $(flags-c-kernel)"

args := $(args-c)
args-kernel  := $(args-c-kernel)

dir := dir-obj="$(path-objects)/bcl/$(architecture)" dir-src="$(path-d)"
dir-kernel := dir-obj="$(path-objects)/kernel/bcl/$(architecture)" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-s)=$(target) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)
	$(q)$(MAKE) $(builder-s)=$(target-kernel) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir-kernel) $(args-kernel) $(target-kernel)