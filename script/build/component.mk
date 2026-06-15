include $(global-env)
include $(path-script)/env/local.mk
-include $(path-e)/config.mk

include $(path-e)/flags.mk
include $(path-script)/helper.mk
include $(path-script)/tool.mk
include $(path-script)/build/libc.mk

component-root ?= $(path-d)
component-name ?= $(notdir $(component-root))
component-kind ?=
component-variants ?= default

sources :=
objects :=
deps :=
attachments :=
arch-sources-var := $(architecture)-sources

include $(component-root)/options.mk

component-include-mks ?= $(shell find $(component-root) -name include.mk | sort)

prefix-include-source = $(if $(filter /%,$(1)),$(1),$(if $(include-dir),$(include-dir)/$(1),$(1)))

define include-component-include
include-file := $(1)
include-dir := $(patsubst %/,%,$(patsubst $(component-root)/%,%,$(dir $(1))))
sources-before-include := $$(sources)
arch-sources-before-include := $$($(arch-sources-var))
sources :=
$(arch-sources-var) :=
include $(1)
sources-added := $$(sources)
arch-sources-added := $$($(arch-sources-var))
sources := $$(sources-before-include) $$(foreach source,$$(sources-added),$$(call prefix-include-source,$$(source)))
$(arch-sources-var) := $$(arch-sources-before-include) $$(foreach source,$$(arch-sources-added),$$(call prefix-include-source,$$(source)))
endef

$(foreach include-mk,$(component-include-mks),$(eval $(call include-component-include,$(include-mk))))

sources += $($(arch-sources-var))

source-objects := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.S,%.o,$(sources))))
source-deps := $(patsubst %.c,%.d,$(filter %.c,$(sources))) \
	$(patsubst %.cpp,%.d,$(filter %.cpp,$(sources)))

objects += $(source-objects)
deps += $(source-deps)

include $(path-script)/helper.mk

variant.default.target ?= $(component-target)
variant.default.dir-obj ?= $(component-objdir)
variant.default.flags-c ?=
variant.default.flags-cpp ?=
variant.default.flags-asm ?=
variant.default.flags-ld ?=
variant.default.script-ld ?= $(script-ld)
variant.default.libraries ?= $(libraries)
variant.default.attachments ?= $(attachments)

variant.$(architecture).target ?= $(variant.default.target)
variant.$(architecture).dir-obj ?= $(variant.default.dir-obj)
variant.$(architecture).flags-c ?= $(variant.default.flags-c)
variant.$(architecture).flags-cpp ?= $(variant.default.flags-cpp)
variant.$(architecture).flags-asm ?= $(variant.default.flags-asm)
variant.$(architecture).flags-ld ?= $(variant.default.flags-ld)
variant.$(architecture).script-ld ?= $(variant.default.script-ld)
variant.$(architecture).libraries ?= $(variant.default.libraries)
variant.$(architecture).attachments ?= $(variant.default.attachments)

component-variant-target = $(or $(if $(filter-out default,$(1)),$(variant.$(1).target)),$(variant.$(architecture).target),$(variant.$(1).target))
component-variant-dir-obj = $(or $(if $(filter-out default,$(1)),$(variant.$(1).dir-obj)),$(variant.$(architecture).dir-obj),$(variant.$(1).dir-obj))
component-variant-flags-c = $(or $(if $(filter-out default,$(1)),$(variant.$(1).flags-c)),$(variant.$(architecture).flags-c),$(variant.$(1).flags-c))
component-variant-flags-cpp = $(or $(if $(filter-out default,$(1)),$(variant.$(1).flags-cpp)),$(variant.$(architecture).flags-cpp),$(variant.$(1).flags-cpp))
component-variant-flags-asm = $(or $(if $(filter-out default,$(1)),$(variant.$(1).flags-asm)),$(variant.$(architecture).flags-asm),$(variant.$(1).flags-asm))
component-variant-flags-ld = $(or $(if $(filter-out default,$(1)),$(variant.$(1).flags-ld)),$(variant.$(architecture).flags-ld),$(variant.$(1).flags-ld))
component-variant-script-ld = $(or $(if $(filter-out default,$(1)),$(variant.$(1).script-ld)),$(variant.$(architecture).script-ld),$(variant.$(1).script-ld))
component-variant-libraries = $(if $(filter undefined,$(origin variant.$(architecture).libraries)),$(variant.$(1).libraries),$(variant.$(architecture).libraries))
component-variant-attachments = $(if $(filter undefined,$(origin variant.$(architecture).attachments)),$(variant.$(1).attachments),$(variant.$(architecture).attachments))

component-args-c = defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c) $(call component-variant-flags-c,$(1))"
component-args-cpp = defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp) $(call component-variant-flags-cpp,$(1))"
component-args-asm = defs-asm="$(defs-asm)" include-asm="$(include-asm)" flags-asm="$(flags-asm) $(call component-variant-flags-asm,$(1))"
component-args-ld = flags-ld="$(flags-ld) $(call component-variant-flags-ld,$(1))" script-ld="$(call component-variant-script-ld,$(1))"
component-args = $(call component-args-ld,$(1)) $(call component-args-c,$(1)) $(call component-args-cpp,$(1)) $(call component-args-asm,$(1)) libraries="$(call component-variant-libraries,$(1))"

ifeq ($(component-kind), static-library)

ifeq ($(library-is-libc), true)
component-libc-args := obj-crt0="$(libc-crt0)" obj-crti="$(libc-crti)" obj-crtn="$(libc-crtn)" libc-mode=true
else
component-libc-args :=
endif

define component_static_variant
.PHONY: build-$(component-name)-$(1)
build-$(component-name)-$(1):
	$$(q)$$(MAKE) $$(builder-s)="$(call component-variant-target,$(1))" architecture="$$(architecture)" deps="$$(deps)" objects="$$(objects)" $$(component-libc-args) dir-obj="$(call component-variant-dir-obj,$(1))" dir-src="$$(component-root)" $$(call component-args,$(1)) "$(call component-variant-target,$(1))"
endef

$(foreach variant,$(component-variants),$(eval $(call component_static_variant,$(variant))))

.PHONY: build
build: $(foreach variant,$(component-variants),build-$(component-name)-$(variant))

endif

ifeq ($(component-kind), module)

module-libc ?= $(default-libc)
module-linker-script-path := $(if $(module-linker-script),$(component-root)/$(module-linker-script),$(libc.$(module-libc).module-linker-script))
module-crt-head-objs := $(libc.$(module-libc).crt-head)
module-crt-tail-objs := $(libc.$(module-libc).crt-tail)

variant.default.target := $(path-bin)/mods/$(architecture)/$(module-output)
variant.default.dir-obj := $(path-objects)/module/$(component-name)
variant.default.script-ld := $(module-linker-script-path)
variant.default.libraries := $(module-libraries)

.PHONY: build
build:
	$(q)$(MAKE) $(builder-m)="$(call component-variant-target,default)" architecture="$(architecture)" attachments="$(attachments)" deps="$(deps)" objects="$(objects)" module-crt-head-objs="$(module-crt-head-objs)" module-crt-tail-objs="$(module-crt-tail-objs)" dir-obj="$(call component-variant-dir-obj,default)" dir-src="$(component-root)" $(call component-args,default) "$(call component-variant-target,default)"
	$(q)$(copy) $(call component-variant-target,default) $(path-initrd)/$(module-output)

endif

ifeq ($(component-kind), kernel)

define component_kernel_variant
.PHONY: build-$(component-name)-$(1)
build-$(component-name)-$(1):
	$$(q)$$(MAKE) $$(builder-k)="$(call component-variant-target,$(1))" architecture="$$(architecture)" attachments="$(call component-variant-attachments,$(1))" deps="$$(deps)" objects="$$(objects)" dir-obj="$(call component-variant-dir-obj,$(1))" dir-src="$$(component-root)" $$(call component-args,$(1)) "$(call component-variant-target,$(1))"
endef

$(foreach variant,$(component-variants),$(eval $(call component_kernel_variant,$(variant))))

.PHONY: build
build: $(foreach variant,$(component-variants),build-$(component-name)-$(variant))

endif
