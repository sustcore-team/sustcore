# 标志位
$(path-attach)/%.tar : $(path-bin)/%
	$(call prepare, $@)
	tar -C $(dir $<) -cf $@ $(notdir $<)
# 	cp $@ $(path-bin)