component-root := $(owner-root)
includes-c += -I$(owner-root)
includes-cpp += -I$(owner-root)
includes-asm += -I$(owner-root)
include $(path-s)/build/collector.mk
