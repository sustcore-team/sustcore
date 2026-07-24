.PHONY: configure
config ?= default

configure:
	$(q)$(s-configure) config=$(config) $(if $(arch),arch=$(arch))
