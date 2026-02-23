include $(global-env)

include $(path-script)/env/local.mk

include $(path-script)/helper.mk

.PHONY: build

target := $(path-bin)/libs/$(architecture)/libkmod.a

objects := setup.o
deps := setup.d
obj-crt0 := crt0.o
obj-crti := crti.o
obj-crtn := crtn.o

flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		   -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
		   -fno-stack-protector -Wno-int-to-pointer-cast \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medlow

include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs -I./third_party/include -I./third_party/include/std

defs-c :=

# CPP

flags-cpp := -std=gnu++26 -nostdlib -fno-builtin -ffreestanding -nostdinc \
		   -Wall -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
		   -fno-stack-protector -Wno-int-to-pointer-cast -fno-rtti -fno-exceptions \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medlow
include-third_party-cpp := -I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
						   -I$(path-third_party)/include/std
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
			   $(include-third_party-cpp) -I$(path-d) -I$(path-include)/arch
defs-cpp :=

ifeq ($(build-mode), debug)
	flags-c += -O0 -g
	defs-c +=
	flags-cpp += -O0 -g
	defs-cpp +=
else
	flags-c += -Ofast
	defs-c += -DNDEBUG
	flags-cpp += -Ofast
	defs-cpp += -DNDEBUG
endif

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"
args-cpp := defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp)"

args := $(args-c) $(args-cpp)

dir := dir-obj="$(path-objects)/kmod/$(architecture)" dir-src="$(path-d)"
libc-obj := obj-crt0=$(obj-crt0) obj-crti=$(obj-crti) obj-crtn=$(obj-crtn) libc-mode=true

build:
	$(q)$(MAKE) $(builder-s)=$(target) architecture=$(architecture) deps="$(deps)" $(libc-obj) objects="$(objects)" $(dir) $(args) $(target)