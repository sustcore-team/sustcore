.PHONY: configure
config ?= default

configure:
	$(q)$(s-configure) config=$(config) arch=$(arch)
