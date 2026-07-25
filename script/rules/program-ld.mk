# Input variables:
#   program-id
#
# Produces:
#   archives
#   includes-c
#   includes-cpp
#   includes-asm
#   link-script
#   crt-head
#   crt-tail

program-dep-archives := $(or $($(program-id)-dep-archives-$(arch)),$($(program-id)-dep-archives))
program-c-library-id := $(or $($(program-id)-c-library-id-$(arch)),$($(program-id)-c-library-id))
program-c-library-archive := $(or $($(program-id)-c-library-archive-$(arch)),$($(program-id)-c-library-archive))
program-c-library-ldscript := $(or $($(program-id)-c-library-ldscript-$(arch)),$($(program-id)-c-library-ldscript))
program-c-library-crt0 := $(or $($(program-id)-c-library-crt0-$(arch)),$($(program-id)-c-library-crt0))
program-c-library-crti := $(or $($(program-id)-c-library-crti-$(arch)),$($(program-id)-c-library-crti))
program-c-library-crtn := $(or $($(program-id)-c-library-crtn-$(arch)),$($(program-id)-c-library-crtn))

includes-c += $(or $($(program-id)-includes-c-$(arch)),$($(program-id)-includes-c))
includes-cpp += $(or $($(program-id)-includes-cpp-$(arch)),$($(program-id)-includes-cpp))
includes-asm += $(or $($(program-id)-includes-asm-$(arch)),$($(program-id)-includes-asm))
includes-c += $(or $($(program-id)-c-library-includes-c-$(arch)),$($(program-id)-c-library-includes-c))
includes-cpp += $(or $($(program-id)-c-library-includes-cpp-$(arch)),$($(program-id)-c-library-includes-cpp))
includes-asm += $(or $($(program-id)-c-library-includes-asm-$(arch)),$($(program-id)-c-library-includes-asm))

archives += $(filter-out $(program-c-library-archive),$(program-dep-archives))
archives += $(program-c-library-archive)

link-script ?= $(or $($(program-id)-ldscript),$(program-c-library-ldscript))

ifneq ($(strip $(program-c-library-id)),)
crt-head := $(program-c-library-crt0) $(program-c-library-crti)
crt-tail := $(program-c-library-crtn)
endif
