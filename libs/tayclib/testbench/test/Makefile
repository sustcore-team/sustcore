ifeq ($(program-id),tayclib-itoa-test)
sources-c := itoa_test.c
else
$(error unsupported tayclib host test: $(program-id))
endif

include $(global-env)
include $(path-s)/host/program.mk

.PHONY: build
build: $(target)
