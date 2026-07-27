bear ?= bear
compile-commands-root := $(path-build-root)
compile-commands-file = $(path-build)/compile_commands.json
host-compile-commands-temp = $(path-build)/.compile_commands.bear.json

.PHONY: select-compile-commands clangd-target clangd-host
.PHONY: update update-host _select-host-compile-commands
.PHONY: _host-compdb _update-host-compile-commands

select-compile-commands:
	$(q)$(s-compile-commands) select \
		"root=$(compile-commands-root)" \
		"arch=$(cached-arch)" \
		"mode=$(cached-mode)"

clangd-target:
	$(q)$(s-compile-commands) select \
		"root=$(compile-commands-root)" \
		"arch=$(arch)" \
		"mode=$(mode)"

_select-host-compile-commands:
	$(q)$(s-compile-commands) select-host \
		"root=$(compile-commands-root)" \
		"triple=$(host-triple)" \
		"mode=$(mode)" \
		"sanitize=$(sanitize)"

clangd-host:
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 \
		allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) \
		_select-host-compile-commands

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

_host-compdb: _build-host-libs $(addprefix host-program-,$(testbench-program-ids))
	$(q)$(echo) "All host compilation database inputs built"

_update-host-compile-commands:
	$(q)$(s-compile-commands) prepare-host \
		"root=$(compile-commands-root)" \
		"triple=$(host-triple)" \
		"mode=$(mode)" \
		"sanitize=$(sanitize)"
	$(q)$(rm) $(host-compile-commands-temp)
	$(q)$(bear) --output "$(host-compile-commands-temp)" -- \
		$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 \
		allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) -B _host-compdb
	$(q)$(s-compile-commands) publish-host \
		"root=$(compile-commands-root)" \
		"triple=$(host-triple)" \
		"mode=$(mode)" \
		"sanitize=$(sanitize)" \
		"source=$(host-compile-commands-temp)"

update-host:
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 \
		allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 \
		allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) \
		_update-host-compile-commands
