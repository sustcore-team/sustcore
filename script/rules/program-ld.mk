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

program-dep-archives := $($(owner-id)-dep-archives)
program-c-library-id := $($(owner-id)-c-library-id)
program-c-library-archive := $($(owner-id)-c-library-archive)
program-c-library-ldscript := $($(owner-id)-c-library-ldscript)
program-c-library-crt0 := $($(owner-id)-c-library-crt0)
program-c-library-crti := $($(owner-id)-c-library-crti)
program-c-library-crtn := $($(owner-id)-c-library-crtn)

includes-c += $($(owner-id)-includes-c)
includes-cpp += $($(owner-id)-includes-cpp)
includes-asm += $($(owner-id)-includes-asm)
includes-c += $($(owner-id)-c-library-includes-c)
includes-cpp += $($(owner-id)-c-library-includes-cpp)
includes-asm += $($(owner-id)-c-library-includes-asm)

archives += $(filter-out $(program-c-library-archive),$(program-dep-archives))
archives += $(program-c-library-archive)

link-script ?= $(or $($(owner-id)-ldscript),$(program-c-library-ldscript))

ifneq ($(strip $(program-c-library-id)),)
crt-head := $(program-c-library-crt0) $(program-c-library-crti)
crt-tail := $(program-c-library-crtn)
endif
