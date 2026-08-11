# Host sub-makes use a validated native toolchain and never inherit cached-arch.
-include $(path-cache)/.switch.mk
include $(path-cache)/host.mk

ifneq ($(and $(filter command line environment,$(origin arch)),$(if $(allow-target-arch),,y)),)
$(error host builds do not accept arch=; use host-arch= on the top-level host target)
endif

mode ?= $(cached-mode)
ifeq ($(strip $(mode)),)
mode := debug
endif

sanitize ?=
host-valid-sanitizers := address undefined address,undefined
ifneq ($(strip $(sanitize)),)
ifeq ($(filter $(sanitize),$(host-valid-sanitizers)),)
$(error unsupported sanitize= value '$(sanitize)'; expected address, undefined, or address,undefined)
endif
endif

override arch := $(host-arch)
path-build-root ?= $(path-e)/build
path-build := $(path-build-root)/$(mode)/host/$(host-triple)
comma := ,
ifneq ($(strip $(sanitize)),)
sanitize-profile := $(subst $(comma),-,$(sanitize))
path-build := $(path-build)/sanitize/$(sanitize-profile)
endif
path-bin ?= $(path-build)/bin
path-obj ?= $(path-build)/obj
path-test ?= $(path-build)/test
path-bench ?= $(path-build)/bench
path-example ?= $(path-build)/example
path-host-tool ?= $(path-build)/host-tool
path-host-tool-publish ?= $(path-build-root)/$(mode)/host-tool
toolchain-stamp := $(path-build)/.toolchain-$(host-toolchain-fingerprint)

include $(path-s)/env/selection.mk
include $(path-s)/env/q.mk

$(toolchain-stamp):
	$(q)$(mkdir) $(@D)
	$(q)touch $@
