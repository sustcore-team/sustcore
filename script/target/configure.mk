.PHONY: configure

configure:
	$(q)$(s-configure) config=$(config) $(if $(arch),arch=$(arch))
