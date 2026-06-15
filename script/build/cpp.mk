# 标志位
flags-cpp ?=
defs-cpp ?=
include-cpp ?=

$(dir-obj)/%.o : $(dir-src)/%.cpp
	$(call prepare, $@)
	$(q)$(compiler-cpp) -c -o $@ $(flags-cpp) $(defs-cpp) -D__ARCHITECTURE__=$(architecture) -D__ARCH_$(architecture)__=1 $(include-cpp) $<

$(dir-obj)/%.d : $(dir-src)/%.cpp
	$(call prepare, $@)
	$(q)$(compiler-cpp) -MM -MP -MF $@.tmp -MT $(basename $@).o -MT $@ $(flags-cpp) $(defs-cpp) -D__ARCHITECTURE__=$(architecture) -D__ARCH_$(architecture)__=1 $(include-cpp) $<
	$(q)mv $@.tmp $@
