# Archive selection for freestanding targets and the validated host.
include $(path-s)/toolchain/environment.mk

$(is-host)-toolchain-comp-ar := $(host-comp-ar)
$(is-freestanding)-toolchain-comp-ar := llvm-ar
comp-ar ?= $(y-toolchain-comp-ar)

# Common archive flags.
flags-ar ?= rcs
