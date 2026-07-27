ifeq ($(program-id),taycpplib-behavior-test)
sources-cpp := behavior_test.cpp
else ifeq ($(program-id),taycpplib-panic-test)
sources-cpp := panic_test.cpp
else
$(error unsupported taycpplib host test: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
