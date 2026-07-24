# mini-cstd compile flags and include paths.

flags-c += -fPIC

minicstd-root ?= $(path-e)/libs/mincstd
gcccheaders-root ?= $(path-e)/third_party/libs/gcc-cheaders

includes-c += -I$(minicstd-root)/include
includes-c += -I$(gcccheaders-root)/include
