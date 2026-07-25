# Input variables:
#   target
#   objects
#   comp-ld
#   flags-ld
#   link-script
#   archives
#   crt-head
#   crt-tail
#   mkdir

link-script-arg := $(if $(link-script),-T $(link-script))
link-inputs := $(crt-head) $(objects) $(archives) $(crt-tail)

$(target): $(objects) $(crt-head) $(crt-tail)
	$(mkdir) $(@D)
	$(q)$(comp-ld) $(flags-ld) $(link-script-arg) -L$(path-bin)/libs -o $@ $(link-inputs)
