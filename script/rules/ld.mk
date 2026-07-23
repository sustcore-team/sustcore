# Input variables:
#   target
#   objects
#   comp-ld
#   flags-ld
#   link-script
#   libraries
#   mkdir

libraries-ld := $(foreach library,$(libraries),-l$(library))
link-script-arg := $(if $(link-script),-T $(link-script))

$(target): $(objects)
	$(mkdir) $(@D)
	$(q)$(comp-ld) $(flags-ld) $(link-script-arg) -L$(path-bin)/libs/$(arch) -o $@ $(objects) $(libraries-ld)
