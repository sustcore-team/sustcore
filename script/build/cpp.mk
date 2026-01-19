# 标志位
flags-cpp ?=
defs-cpp ?=
include-cpp ?=

$(dir-obj)/%.o : $(dir-src)/%.cpp
	$(call prepare, $@)
	$(q)$(compiler-cpp) -c -o $@ $(flags-cpp) $(defs-cpp) -D__ARCHITECTURE__=$(architecture) $(include-cpp) $<

$(dir-obj)/%.d : $(dir-src)/%.cpp
	$(call prepare, $@)
	$(q)$(compiler-cpp) -MM -o $@.tmp $(flags-cpp) $(defs-cpp) -D__ARCHITECTURE__=$(architecture) $(include-cpp) $<
	$(q)sed -E 's|[a-zA-Z0-9]+\.o| $(basename $@).o $(basename $@).d |g' <$@.tmp >$@
	$(q)rm -f $@.tmp