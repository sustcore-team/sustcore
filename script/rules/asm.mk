# Input variables:
#   src-root
#   obj-root
#   sources-asm
#   comp-asm
#   flags-asm
#   macros-asm
#   includes-asm
#   mkdir

$(obj-root)/%.o: $(src-root)/%.S
	$(q)$(mkdir) $(@D)
	$(q)$(comp-asm) -x assembler-with-cpp -c -o $@ $(flags-asm) $(macros-asm) $(includes-asm) $<
