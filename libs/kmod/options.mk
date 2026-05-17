component-kind := static-library
component-name := kmod
component-target := $(path-bin)/libs/$(architecture)/libkmod.a
component-objdir := $(path-objects)/kmod/$(architecture)

library-is-libc := true
libc-crt0 := crt0.o
libc-crti := crti.o
libc-crtn := crtn.o

flags-c := $(flags-common-c) -nostdinc $(flags-mode-c)
include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs \
	-I$(path-third_party)/include -I$(path-third_party)/include/std
defs-c := $(defs-mode-c)

flags-cpp := $(flags-common-cpp) -nostdinc $(flags-no-rtti-cpp) $(flags-no-exceptions-cpp) \
	$(flags-mode-cpp)
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
	-I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
	-I$(path-third_party)/include/std -I$(component-root) -I$(path-include)/arch
defs-cpp := $(defs-mode-cpp)

include-asm := -I$(path-include) -I$(path-include)/std \
	-I$(path-third_party)/include -I$(path-third_party)/include/std
