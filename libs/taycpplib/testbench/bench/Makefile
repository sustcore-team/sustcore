ifeq ($(program-id),taycpplib-range-bench)
sources-cpp := range.cpp
else ifeq ($(program-id),taycpplib-string-view-bench)
sources-cpp := string_view.cpp
else ifeq ($(program-id),taycpplib-sort-bench)
sources-cpp := sort.cpp
else
$(error unsupported taycpplib host benchmark: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
