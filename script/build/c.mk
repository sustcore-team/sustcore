# 标志位
flags-c ?=
defs-c ?=
include-c ?=

obj-crtbegin-libc := $(shell $(compiler-c) -print-file-name=crtbegin.o)
obj-crtend-libc := $(shell $(compiler-c) -print-file-name=crtend.o)

$(dir-obj)/%.o : $(dir-src)/%.c
	$(call prepare, $@)
	$(q)$(compiler-c) -c -o $@ $(flags-c) $(defs-c) -D__ARCHITECTURE__=$(architecture) $(include-c) $<

$(dir-obj)/%.d : $(dir-src)/%.c
	$(call prepare, $@)
	$(q)$(compiler-c) -MM -o $@.tmp $(flags-c) $(defs-c) -D__ARCHITECTURE__=$(architecture) $(include-c) $<
	$(q)sed -E 's|[a-zA-Z0-9]+\.o| $(basename $@).o $(basename $@).d |g' <$@.tmp >$@
	$(q)rm -f $@.tmp