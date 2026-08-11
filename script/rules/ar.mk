# Input variables:
#   target
#   ar-target (optional; defaults to target)
#   objects
#   comp-ar
#   flags-ar
#   mkdir

ar-target ?= $(target)

$(ar-target): $(objects)
	$(q)$(mkdir) $(@D)
	$(q)$(comp-ar) $(flags-ar) $@ $(objects)
