include $(global-env)

include $(path-script)/env/local.mk
include $(path-script)/helper.mk
include $(path-script)/tool.mk

_output := default.mod
target := $(path-bin)/mods/$(architecture)/$(_output)
initrd-target := $(path-bin)/initrd/$(_output)

objects := main.o
deps := main.d

subdirs :=

include $(foreach subdir, $(subdirs), $(path-d)/$(subdir)/include.mk)

flags-ld := -z max-page-size=0x100 -nostdlib -e_start

# C
flags-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding -nostdinc++ \
		     -Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
		     -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
		     -fno-stack-protector -Wno-int-to-pointer-cast \
		     -fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medany

include-third_party-c := -I$(path-third_party)/include -I$(path-third_party)/include/libfdt -I$(path-third_party)/include/std
include-c := -I$(path-include) -I$(path-include)/std $(include-third_party-c) -I$(path-d) -I$(path-include)/arch

defs-c := -DASSERT_IMPLEMENTED=0

# CPP
flags-cpp := -std=gnu++26 -nostdlib -ffreestanding -nostdinc \
		   -Wall -Wno-sign-compare -fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
 		   -fno-stack-protector -Wno-int-to-pointer-cast -fno-rtti -fno-exceptions \
		   -fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medany

include-third_party-cpp := -I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
						   -I$(path-third_party)/include/std
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
			   $(include-third_party-cpp) -I$(path-d) -I$(path-include)/arch
defs-cpp := -DASSERT_IMPLEMENTED=0

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

libs := basecpp kmod

args-ld := flags-ld="$(flags-ld)"

args-c := defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c)"
args-cpp := defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp)"
args-asm := flags-asm="$(flags-asm)"

args := $(args-ld) $(args-c) $(args-cpp) $(args-asm) libraries="$(libs)"

dir := dir-obj="$(path-objects)/module/default" dir-src="$(path-d)"

.PHONY: build
build:
	$(q)$(MAKE) $(builder-k)=$(target) architecture=$(architecture) attachments="$(attachments)" deps="$(deps)" objects="$(objects)" $(dir) $(args) $(target)
	$(q)$(copy) $(target) $(initrd-target)