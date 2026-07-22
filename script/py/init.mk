.PHONY: init-py-scripts
init-py-scripts: | $(path-cache)
	$(q)$(echo) "Changing permission of $(s-switch) to executable"
	$(q)$(chmode) +x $(s-switch)
	$(q)$(echo) "Changing permission of $(s-configure) to executable"
	$(q)$(chmode) +x $(s-configure)

$(path-cache):
	$(q)$(mkdir) $@
