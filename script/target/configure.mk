.PHONY: configure

configure:
	$(q)$(s-configure) config=$(config) $(if $(arch),arch=$(arch))
	$(q)$(MAKE) --no-print-directory select-compile-commands
