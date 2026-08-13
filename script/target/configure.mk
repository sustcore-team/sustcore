.PHONY: configure

configure:
	$(q)$(s-configure) config=$(config) \
		$(if $(filter command line,$(origin arch)),arch=$(arch)) \
		$(if $(filter command line,$(origin mode)),mode=$(mode))
	$(q)$(if $(and $(cached-arch),$(cached-mode)),$(MAKE) --no-print-directory select-compile-commands,)
