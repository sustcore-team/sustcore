component-kind := static-library
component-name := basecpp
component-variants := default kernel

variant.default.target := $(path-bin)/libs/$(architecture)/libbasecpp.a
variant.default.dir-obj := $(path-objects)/bcl/$(architecture)
variant.kernel.target := $(path-bin)/libs/$(architecture)/libkerbasecpp.a
variant.kernel.dir-obj := $(path-objects)/kernel/bcl/$(architecture)
variant.kernel.flags-cpp := $(flags-no-rtti-cpp) $(flags-no-exceptions-cpp) -DUSE_SUSTCORE_FEATURES

flags-c := $(flags-common-c) -nostdinc $(flags-mode-c)
include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs \
	-I$(path-third_party)/include -I$(path-third_party)/include/std
defs-c := $(defs-mode-c)

flags-cpp := $(flags-common-cpp) -nostdinc $(flags-mode-cpp)
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
	-I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
	-I$(path-third_party)/include/std -I$(component-root) -I$(path-include)/arch
defs-cpp := $(defs-mode-cpp)

# ifeq ($(architecture),loongarch64)
# flags-cpp += -mno-lsx
# endif