# Kernel target skeleton driven by the top-level Makefile.
ifeq ($(origin global-env), undefined)
$(error kernel/Makefile must be invoked with global-env from the top-level Makefile)
endif
ifeq ($(origin kernel-path), undefined)
$(error kernel/Makefile must be invoked with kernel-path from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
-include $(path-cache)/deps-kernel.mk

kernel-root := $(path-e)/kernel
kernel-obj-root := $(path-obj)/kernel

# Load the architecture defaults first, then normalize the externally selected
# boot method.
include $(kernel-root)/variant.$(arch).mk
include $(kernel-root)/enable.mk

src-root := $(kernel-root)
obj-root := $(kernel-obj-root)
target := $(kernel-path)
link-script ?= $(boot-link-script)

include $(kernel-root)/flags.mk
include $(kernel-root)/include.mk

# Resolve the configured toolchain into the language-neutral rule interface.
include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ld.mk

comp-c ?= $($(arch)-comp-c)
comp-cpp ?= $($(arch)-comp-cpp)
comp-asm ?= $(comp-c)
comp-ld ?= $($(arch)-comp-ld)

flags-c += $($(arch)-flags-c)
flags-cpp += $($(arch)-flags-cpp)
flags-asm += $($(arch)-flags-c)

libraries += $(or $(kernel-libraries-$(arch)),$(kernel-libraries))
includes-c += $(or $(kernel-includes-c-$(arch)),$(kernel-includes-c))
includes-cpp += $(or $(kernel-includes-cpp-$(arch)),$(kernel-includes-cpp))
includes-asm += $(or $(kernel-includes-asm-$(arch)),$(kernel-includes-asm))

# The generic build driver will consume these variables in the next layer.
sources-asm := $(strip $(sources-y-asm))
sources-c := $(strip $(sources-y-c))
sources-cpp := $(strip $(sources-y-cpp))

objects-asm := $(addprefix $(obj-root)/,$(sources-asm:.S=.o))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects := $(objects-asm) $(objects-c) $(objects-cpp)
deps := $(objects:.o=.d)

include $(path-s)/rules/asm.mk
include $(path-s)/rules/c.mk
include $(path-s)/rules/cpp.mk
include $(path-s)/rules/ld.mk

-include $(deps)

.PHONY: build kernel-vars
build: $(target)

kernel-vars:
	$(q)$(echo) "kernel arch=$(arch) boot=$(if $(filter y,$(enable-sbi)),sbi,laboot)"
	$(q)$(echo) "kernel path=$(kernel-path)"
	$(q)$(echo) "kernel target=$(target)"
	$(q)$(echo) "kernel link-script=$(link-script)"
	$(q)$(echo) "kernel asm sources=$(sources-asm)"
	$(q)$(echo) "kernel cpp sources=$(sources-cpp)"
	$(q)$(echo) "kernel objects=$(objects)"
