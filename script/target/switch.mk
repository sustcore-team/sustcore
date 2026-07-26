.PHONY: switch
switch:
	$(q)$(echo) "Switching build mode to $(mode) and build arch to $(arch)"
	$(q)$(s-switch) mode=$(mode) arch=$(arch)
	$(q)$(MAKE) --no-print-directory select-compile-commands
