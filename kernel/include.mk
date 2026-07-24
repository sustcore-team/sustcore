# Kernel-wide source files and include paths.

stdlib-c       := -I$(path-include) -I$(path-include)/std
stdlib-cpp     := $(stdlib-c) -I$(path-include)/std/c++
thirdparty-c   := -I$(path-third_party)/include
thirdparty-cpp := $(thirdparty-c)

includes-c   += -I$(kernel-root) $(stdlib-c)   $(thirdparty-c)
includes-cpp += -I$(kernel-root) $(stdlib-cpp) $(thirdparty-cpp)
includes-asm += -I$(kernel-root) $(stdlib-c)   $(thirdparty-c)

component-root := $(kernel-root)
include $(path-s)/build/collector.mk
