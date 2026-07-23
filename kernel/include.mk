# Kernel-wide source files and include paths.

stdlib-c       := -I$(path-include) -I$(path-include)/std
stdlib-cpp     := $(stdlib-c) -I$(path-include)/std/c++
libsbi-c       := -I$(path-lib)/sbi/include
libfdt-c       := -I$(path-third_party)/libs/libfdt/include
thirdparty-c   := -I$(path-third_party)/include
thirdparty-cpp := $(thirdparty-c)

includes-c   += -I$(kernel-root) $(stdlib-c)   $(thirdparty-c)   $(libsbi-c) $(libfdt-c)
includes-cpp += -I$(kernel-root) $(stdlib-cpp) $(thirdparty-cpp) $(libsbi-c) $(libfdt-c)
includes-asm += -I$(kernel-root) $(stdlib-c)   $(thirdparty-c)   $(libsbi-c) $(libfdt-c)

component-root := $(kernel-root)
include $(path-s)/build/collector.mk
