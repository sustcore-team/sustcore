ifeq ($(program-id),tayeffect-effect-example)
sources-cpp := effect.cpp
else ifeq ($(program-id),tayeffect-staged-example)
sources-cpp := staged.cpp
else ifeq ($(program-id),tayeffect-effect-staged-example)
sources-cpp := effect_staged.cpp
else
$(error unsupported tayeffect host example: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
