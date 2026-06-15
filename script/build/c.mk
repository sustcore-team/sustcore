# 标志位
flags-c ?=
defs-c ?=
include-c ?=

obj-crtbegin-libc := $(shell $(compiler-c) -print-file-name=crtbegin.o)
obj-crtend-libc := $(shell $(compiler-c) -print-file-name=crtend.o)

$(dir-obj)/%.o : $(dir-src)/%.c
	$(call prepare, $@)
	$(q)$(compiler-c) -c -o $@ $(flags-c) $(defs-c) -D__ARCHITECTURE__=$(architecture) -D__ARCH_$(architecture)__=1 $(include-c) $<

$(dir-obj)/%.d : $(dir-src)/%.c
	$(call prepare, $@)
	$(q)$(compiler-c) -MM -MP -MF $@.tmp -MT $(basename $@).o -MT $@ $(flags-c) $(defs-c) -D__ARCHITECTURE__=$(architecture) -D__ARCH_$(architecture)__=1 $(include-c) $<
	$(q)mv $@.tmp $@
