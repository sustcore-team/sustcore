# Input variables:
#   target
#   ld-target (optional; defaults to target)
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
ld-target ?= $(target)

$(ld-target): $(objects) $(crt-head) $(crt-tail) $(link-script) $(archives)
	$(q)$(mkdir) $(@D)
	$(q)$(comp-ld) $(flags-ld) $(link-script-arg) -L$(path-bin)/libs -o $@ $(link-inputs)
