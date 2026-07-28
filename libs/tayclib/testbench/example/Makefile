ifeq ($(program-id),tayclib-itoa-example)
sources-c := itoa_example.c
else
$(error unsupported tayclib host example: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
