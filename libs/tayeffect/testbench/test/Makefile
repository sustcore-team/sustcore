ifeq ($(program-id),tayeffect-effect-test)
sources-cpp := effect.cpp
else ifeq ($(program-id),tayeffect-staged-test)
sources-cpp := staged.cpp
else ifeq ($(program-id),tayeffect-integration-test)
sources-cpp := integration.cpp
else
$(error unsupported tayeffect host test: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
