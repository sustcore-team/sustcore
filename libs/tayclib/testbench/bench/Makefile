ifeq ($(program-id),tayclib-itoa-bench)
sources-c := itoa_bench.c
else
$(error unsupported tayclib host benchmark: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
