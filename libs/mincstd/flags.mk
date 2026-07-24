# mini-cstd compile flags and include paths.

flags-c += -fPIC

includes-c += -I$(path-include)
includes-c += -I$(path-include)/std
includes-c += -I$(path-third_party)/include
includes-c += -I$(path-third_party)/include/std
