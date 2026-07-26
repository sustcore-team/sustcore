bear ?= bear
compile-commands-root := $(path-build-root)
compile-commands-file = $(path-build)/compile_commands.json

.PHONY: select-compile-commands update

select-compile-commands:
	$(q)$(s-compile-commands) select \
		"root=$(compile-commands-root)" \
		"arch=$(cached-arch)" \
		"mode=$(cached-mode)"

update:
	$(q)$(s-compile-commands) prepare \
		"root=$(compile-commands-root)" \
		"arch=$(arch)" \
		"mode=$(mode)"
	$(q)$(bear) --output "$(compile-commands-file)" -- \
		make build-kernel -B arch="$(arch)" mode="$(mode)"
	$(q)$(s-compile-commands) select \
		"root=$(compile-commands-root)" \
		"arch=$(cached-arch)" \
		"mode=$(cached-mode)"
	$(q)$(echo) "Updated compilation database: $(compile-commands-file)"
