initrd-config := $(path-e)/kernel/initrd.toml
initrd-module-ids := $(shell $(s-initrd) list-modules config=$(initrd-config))
initrd-module-targets := $(addprefix build-module-,$(initrd-module-ids))

.PHONY: build-modules $(initrd-module-targets) build-initrd

build-modules: build-libs $(initrd-module-targets)

$(initrd-module-targets): build-module-%: build-libs
	$(q)$(MAKE) -f $(program-$*-makefile) \
		global-env=$(global-env) \
		arch=$(arch) \
		q=$(q) \
		ctx=$(path-ctx)/module-$*.mk \
		$(program-$*-target)

build-initrd: build-modules
	$(q)$(s-initrd) \
		config=$(initrd-config) \
		path-bin=$(path-bin) \
		path-initrd-root=$(path-initrd-root) \
		path-initrd=$(path-initrd)
