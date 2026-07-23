# Input variables:
#   src-root
#   obj-root
#   sources-c
#   comp-c
#   flags-c
#   macros-c
#   includes-c
#   mkdir

$(obj-root)/%.o: $(src-root)/%.c
	$(mkdir) $(@D)
	$(q)$(comp-c) -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ $(flags-c) $(macros-c) $(includes-c) $<
