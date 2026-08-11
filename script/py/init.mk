.PHONY: init-py-scripts
init-py-scripts: | $(path-cache)
	$(q)$(echo) "Changing permission of Python entry points to executable"
	$(q)$(chmode) +x $(path-sp)/*.py

$(path-cache):
	$(q)$(mkdir) $@
