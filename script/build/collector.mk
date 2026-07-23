# Input variables:
#   component-root
# Optional:
#   component-include-mks
#
# Subdirectory include.mk files may declare:
#   src-y
#   src-n
#
# The collector prefixes each declared source with the relative directory of
# the contributing include.mk and classifies the result by source language.

component-root := $(patsubst %/,%,$(component-root))
component-root-include := $(component-root)/include.mk
component-include-mks ?= $(shell find $(component-root) -mindepth 2 -name include.mk | sort)

sources-y-asm ?=
sources-y-c ?=
sources-y-cpp ?=
sources-n-asm ?=
sources-n-c ?=
sources-n-cpp ?=

prefix-src = $(if $(filter /%,$(2)),$(2),$(if $(1),$(1)/$(2),$(2)))

define collect-component-include
collector-include-file := $(1)
collector-include-dir := $$(patsubst %/,%,$$(patsubst $$(component-root)/%,%,$$(dir $$(collector-include-file))))
src-y :=
src-n :=
include $(1)
collector-src-y-added := $$(foreach src,$$(src-y),$$(call prefix-src,$$(collector-include-dir),$$(src)))
collector-src-n-added := $$(foreach src,$$(src-n),$$(call prefix-src,$$(collector-include-dir),$$(src)))
sources-y-asm := $$(sources-y-asm) $$(filter %.S,$$(collector-src-y-added))
sources-y-c := $$(sources-y-c) $$(filter %.c,$$(collector-src-y-added))
sources-y-cpp := $$(sources-y-cpp) $$(filter %.cpp,$$(collector-src-y-added))
sources-n-asm := $$(sources-n-asm) $$(filter %.S,$$(collector-src-n-added))
sources-n-c := $$(sources-n-c) $$(filter %.c,$$(collector-src-n-added))
sources-n-cpp := $$(sources-n-cpp) $$(filter %.cpp,$$(collector-src-n-added))
endef

$(foreach include-mk,$(filter-out $(component-root-include),$(component-include-mks)),$(eval $(call collect-component-include,$(include-mk))))
