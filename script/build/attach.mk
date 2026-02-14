# 标志位
$(dir-obj)/attachment/%.attachment.o : $(path-attach)/%
	$(call prepare, $@)
	$(call make-attachment, $<, $@)