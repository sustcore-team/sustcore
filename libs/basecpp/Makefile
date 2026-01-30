include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/libs/$(architecture)/libbasecpp.a
target-kernel := $(path-bin)/libs/$(architecture)/libkerbasecpp.a

objects := baseio.o tostring.o string.o ctype.o stdlib.o
deps := baseio.d tostring.d string.d ctype.d stdlib.d

flags-c-kernel := -g -mcmodel=medany
flags-c-non-kernel := -g -mcmodel=medlow

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os

include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs -I./third_party/include -I./third_party/include/std

defs-c :=

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c) $(flags-c-non-kernel)"
args-c-kernel := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c) $(flags-c-kernel)"

# CPP

flags-cpp := -std=gnu++26 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast -fno-rtti -fno-exceptions \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -Os
flags-cpp += -g -mcmodel=medany
include-third_party-cpp := -I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
						   -I$(path-third_party)/include/std
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
			   $(include-third_party-cpp) -I$(path-d) -I$(path-include)/arch
defs-cpp :=
args-cpp := defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp)"

args := $(args-c) $(args-cpp)
args-kernel  := $(args-c-kernel) $(args-cpp)

dir := dir-obj="$(path-objects)/bcl/$(architecture)" dir-src="$(path-d)"
dir-kernel := dir-obj="$(path-objects)/kernel/bcl/$(architecture)" dir-src="$(path-d)"

build:
	$(q)$(MAKE) $(builder-s)=$(target) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)
	$(q)$(MAKE) $(builder-s)=$(target-kernel) architecture=$(architecture) deps="$(deps)" objects="$(objects)" $(dir-kernel) $(args-kernel) $(target-kernel)