# Input variables:
#   target
#   objects
#   comp-ar
#   flags-ar
#   mkdir

$(target): $(objects)
	$(mkdir) $(@D)
	$(q)$(comp-ar) $(flags-ar) $@ $(objects)
