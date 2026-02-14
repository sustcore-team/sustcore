# 标志位
$(path-attach)/%.tar : $(path-bin)/%
	$(call prepare, $@)
	tar -cf $@ $<