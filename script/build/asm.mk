# 标志位
flags-asm ?=
defs-asm ?=
include-asm ?=
flags-asm-wa = $(foreach flag,$(flags-asm),-Wa,$(flag))

$(dir-obj)/%.o : $(dir-src)/%.S
	$(call prepare, $@)
	$(q)$(compiler-c) -x assembler-with-cpp -c -o $@ $(flags-asm-wa) $(defs-asm) $(include-asm) $^
