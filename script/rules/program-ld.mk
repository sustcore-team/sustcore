# Input variables:
#   owner-id
#
# Produces:
#   archives
#   includes-c
#   includes-cpp
#   includes-asm
#   link-script
#   crt-head
#   crt-tail

program-dep-archives := $(or $($(owner-id)-dep-archives-$(arch)),$($(owner-id)-dep-archives))
program-c-library-id := $(or $($(owner-id)-c-library-id-$(arch)),$($(owner-id)-c-library-id))
program-c-library-archive := $(or $($(owner-id)-c-library-archive-$(arch)),$($(owner-id)-c-library-archive))
program-c-library-ldscript := $(or $($(owner-id)-c-library-ldscript-$(arch)),$($(owner-id)-c-library-ldscript))
program-c-library-crt0 := $(or $($(owner-id)-c-library-crt0-$(arch)),$($(owner-id)-c-library-crt0))
program-c-library-crti := $(or $($(owner-id)-c-library-crti-$(arch)),$($(owner-id)-c-library-crti))
program-c-library-crtn := $(or $($(owner-id)-c-library-crtn-$(arch)),$($(owner-id)-c-library-crtn))

includes-c += $(or $($(owner-id)-includes-c-$(arch)),$($(owner-id)-includes-c))
includes-cpp += $(or $($(owner-id)-includes-cpp-$(arch)),$($(owner-id)-includes-cpp))
includes-asm += $(or $($(owner-id)-includes-asm-$(arch)),$($(owner-id)-includes-asm))
includes-c += $(or $($(owner-id)-c-library-includes-c-$(arch)),$($(owner-id)-c-library-includes-c))
includes-cpp += $(or $($(owner-id)-c-library-includes-cpp-$(arch)),$($(owner-id)-c-library-includes-cpp))
includes-asm += $(or $($(owner-id)-c-library-includes-asm-$(arch)),$($(owner-id)-c-library-includes-asm))

archives += $(filter-out $(program-c-library-archive),$(program-dep-archives))
archives += $(program-c-library-archive)

link-script ?= $(or $($(owner-id)-ldscript),$(program-c-library-ldscript))

ifneq ($(strip $(program-c-library-id)),)
crt-head := $(program-c-library-crt0) $(program-c-library-crti)
crt-tail := $(program-c-library-crtn)
endif
