# Static archive layer built on the common component compilation rules.
static-library-mk := $(lastword $(MAKEFILE_LIST))
static-library-dir := $(patsubst %/,%,$(dir $(abspath $(static-library-mk))))
component-repo-root ?= $(abspath $(component-root)/../..)

include $(static-library-dir)/component.mk
include $(path-s)/toolchain/ar.mk
include $(path-s)/rules/ar.mk

.PHONY: build-static static-library-vars
build-static: $(target)

static-library-vars: component-vars
	$(q)$(echo) "archive=$(target)"
