# Kernel-wide source files and include paths.

stdlib-c       := -I$(path-include) -I$(path-include)/std
stdlib-cpp     := $(stdlib-c) -I$(path-include)/std/c++
thirdparty-c   := -I$(path-third_party)/include
thirdparty-cpp := $(thirdparty-c)

includes-c   += -I$(owner-root) $(stdlib-c)   $(thirdparty-c)
includes-cpp += -I$(owner-root) $(stdlib-cpp) $(thirdparty-cpp)
includes-asm += -I$(owner-root) $(stdlib-c)   $(thirdparty-c)

component-root := $(owner-root)
include $(path-s)/build/collector.mk
