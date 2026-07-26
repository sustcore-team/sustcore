# Kernel target skeleton driven by the top-level Makefile.
ifeq ($(origin global-env), undefined)
$(error kernel/Makefile must be invoked with global-env from the top-level Makefile)
endif
ifeq ($(origin build-header), undefined)
$(error kernel/Makefile must be invoked with build-header from the top-level Makefile)
endif

include $(global-env)
include $(path-s)/env/buildpath.mk
include $(build-header)
-include $(path-cache)/deps-$(owner-id).mk

# Load the architecture defaults first, then normalize the externally selected
# boot method.
include $(owner-root)/variant.$(arch).mk
include $(owner-root)/enable.mk

src-root := $(owner-root)
link-script ?= $(boot-link-script)

include $(owner-root)/flags.mk
include $(owner-root)/collect.mk

# Resolve the configured toolchain into the language-neutral rule interface.
include $(path-s)/toolchain/c.mk
include $(path-s)/toolchain/cpp.mk
include $(path-s)/toolchain/ld.mk

riscv64-objcopy-output := elf64-littleriscv
riscv64-objcopy-arch := riscv
loongarch64-objcopy-output := elf64-loongarch
loongarch64-objcopy-arch := loongarch

comp-c ?= $($(arch)-comp-c)
comp-cpp ?= $($(arch)-comp-cpp)
comp-asm ?= $(comp-c)
comp-ld ?= $($(arch)-comp-ld)
comp-objcopy ?= llvm-objcopy

flags-c += $($(arch)-flags-c)
flags-cpp += $($(arch)-flags-cpp)
flags-asm += $($(arch)-flags-c)

archives += $(or $($(owner-id)-dep-archives-$(arch)),$($(owner-id)-dep-archives))
includes-c += $(or $($(owner-id)-includes-c-$(arch)),$($(owner-id)-includes-c))
includes-cpp += $(or $($(owner-id)-includes-cpp-$(arch)),$($(owner-id)-includes-cpp))
includes-asm += $(or $($(owner-id)-includes-asm-$(arch)),$($(owner-id)-includes-asm))

# The generic build driver will consume these variables in the next layer.
sources-asm := $(strip $(sources-y-asm))
sources-c := $(strip $(sources-y-c))
sources-cpp := $(strip $(sources-y-cpp))

objects-asm := $(addprefix $(obj-root)/,$(sources-asm:.S=.o))
objects-c := $(addprefix $(obj-root)/,$(sources-c:.c=.o))
objects-cpp := $(addprefix $(obj-root)/,$(sources-cpp:.cpp=.o))
objects-attachment := $(path-initrd-obj)
objects := $(objects-asm) $(objects-c) $(objects-cpp) $(objects-attachment)
deps := $(objects:.o=.d)

include $(path-s)/rules/asm.mk
include $(path-s)/rules/c.mk
include $(path-s)/rules/cpp.mk
include $(path-s)/rules/ld.mk

-include $(deps)

$(path-initrd-obj): $(path-initrd)
	$(mkdir) $(@D)
	$(q)$(comp-objcopy) -I binary -O $($(arch)-objcopy-output) -B $($(arch)-objcopy-arch) --rename-section .data=.attach.initrd $< $@

.PHONY: build kernel-vars
build: $(target)

kernel-vars:
	$(q)$(echo) "kernel arch=$(arch) boot=$(if $(filter y,$(enable-sbi)),sbi,laboot)"
	$(q)$(echo) "kernel target=$(target)"
	$(q)$(echo) "kernel link-script=$(link-script)"
	$(q)$(echo) "kernel asm sources=$(sources-asm)"
	$(q)$(echo) "kernel cpp sources=$(sources-cpp)"
	$(q)$(echo) "kernel objects=$(objects)"
