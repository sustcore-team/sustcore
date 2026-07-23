# Input variables:
#   src-root
#   obj-root
#   sources-cpp
#   comp-cpp
#   flags-cpp
#   macros-cpp
#   includes-cpp
#   mkdir

$(obj-root)/%.o: $(src-root)/%.cpp
	$(mkdir) $(@D)
	$(q)$(comp-cpp) -MMD -MP -MF $(@:.o=.d) -MT $@ -c -o $@ $(flags-cpp) $(macros-cpp) $(includes-cpp) $<
