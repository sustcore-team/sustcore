# Boot selection.
#
# External callers may set enable-sbi or enable-laboot to y/n.
# If neither is specified, .cache/kernel.mk may provide $(arch)-boot.
# If there is no cached boot config, the architecture variant supplies
# the default boot method.

yes = $(words $(filter y 1 yes true,$(strip $(1))))

enable-sbi ?=
enable-laboot ?=

arch-boot := $($(arch)-boot)

ifeq ($(origin enable-sbi), undefined)
ifeq ($(origin enable-laboot), undefined)
ifneq ($(strip $(arch-boot)),)
ifeq ($(arch-boot),sbi)
enable-sbi := y
enable-laboot := n
else ifeq ($(arch-boot),laboot)
enable-sbi := n
enable-laboot := y
else
$(error unsupported boot method for $(arch): $(arch-boot))
endif
endif
endif
endif

enable-sbi ?= n
enable-laboot ?= n

ifeq ($(call yes,$(enable-sbi)),1)
enable-sbi := y
else
enable-sbi := n
endif

ifeq ($(call yes,$(enable-laboot)),1)
enable-laboot := y
else
enable-laboot := n
endif

ifneq ($(call yes,$(enable-sbi) $(enable-laboot)),1)
$(error exactly one boot method must be enabled: enable-sbi=y or enable-laboot=y)
endif

boot-link-script ?= $(owner-root)/boot/$(if $(filter y,$(enable-sbi)),sbi/sbi.ld,laboot/laboot.ld)
