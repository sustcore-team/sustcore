ifeq ($(program-id),taycpplib-bench)
sources-cpp := taycpplib_bench.cpp
else
$(error unsupported taycpplib host benchmark: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
