# Input variables:
#   target
#   objects
#   comp-ld
#   flags-ld
#   link-script
#   archives
#   mkdir

link-script-arg := $(if $(link-script),-T $(link-script))

$(target): $(objects)
	$(mkdir) $(@D)
	$(q)$(comp-ld) $(flags-ld) $(link-script-arg) -L$(path-bin)/libs/$(arch) -o $@ $(objects) $(archives)
