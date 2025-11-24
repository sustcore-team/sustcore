# 标志位
flags-asm ?=
defs-asm ?=
include-asm ?=

$(dir-obj)/attachment/%.attachment.o : $(path-attach)/%
	$(call prepare, $@)
	$(call make-attachment, $<, $@)