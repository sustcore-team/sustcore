ifeq ($(program-id),taycpplib-range-example)
sources-cpp := range.cpp
else ifeq ($(program-id),taycpplib-refc-example)
sources-cpp := refc.cpp
else ifeq ($(program-id),taycpplib-owner-example)
sources-cpp := owner.cpp
else ifeq ($(program-id),taycpplib-expected-example)
sources-cpp := expected.cpp
else ifeq ($(program-id),taycpplib-panic-example)
sources-cpp := panic.cpp
else ifeq ($(program-id),taycpplib-string-example)
sources-cpp := string.cpp
else ifeq ($(program-id),taycpplib-allocator-example)
sources-cpp := allocator.cpp
else
$(error unsupported taycpplib host example: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
