-include $(path-cache)/.configure.mk

HOST_CLANG ?= $(or $(host-config-clang),clang)
HOST_CLANGXX ?= $(or $(host-config-clangxx),clang++)
HOST_LLVM_AR ?= $(or $(host-config-llvm-ar),llvm-ar)
HOST_SYSROOT ?= $(host-config-sysroot)
HOST_CPPSTDLIB ?= $(or $(host-config-cppstdlib),auto)
HOST_CFLAGS ?= $(host-config-cflags)
HOST_CXXFLAGS ?= $(host-config-cxxflags)
HOST_LDFLAGS ?= $(host-config-ldflags)

export HOST_CLANG HOST_CLANGXX HOST_LLVM_AR HOST_SYSROOT HOST_CPPSTDLIB
export HOST_CFLAGS HOST_CXXFLAGS HOST_LDFLAGS

.PHONY: validate-host
validate-host:
	$(if $(and $(filter command line,$(origin arch)),$(if $(allow-target-arch),,y)),$(error validate-host does not accept arch=; use host-arch= instead))
	$(q)$(s-host) validate output=$(path-cache)/host.mk $(if $(host-arch),host-arch=$(host-arch))

.PHONY: _require-lib _require-host-lib _require-host-tool
.PHONY: _prepare-host-deps _prepare-host-tool-deps _prepare-build-hosttool
_require-lib:
	$(if $(strip $(lib)),,$(error missing required lib=<id>))
	$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib)))

_require-host-lib: _require-lib
	$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host))

_require-host-tool:
	$(if $(strip $(tool)),,$(error missing required tool=<id>))
	$(if $(filter $(tool),$(host-tool-ids-all)),,$(error unknown host tool id: $(tool)))

_prepare-host-deps:
	$(q)set -e; $(foreach owner,$(library-ids),$(s-resolve-deps) root=$(path-e) owner=$(owner) environment=host arch=$(host-arch) output=$(path-deps)/host-$(owner).mk;)

selected-host-tool-ids = $(if $(tool),$(tool),$(host-tool-ids))

_prepare-host-tool-deps:
	$(q)set -e; $(foreach owner,$(selected-host-tool-ids),$(s-resolve-deps) root=$(path-e) owner=$(owner) environment=host arch=$(host-arch) output=$(path-deps)/host-$(owner).mk;)

_prepare-build-hosttool:
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) tool= _prepare-host-tool-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=1 mode=$(mode) sanitize=$(sanitize) _build-host-libs

selected-host-test-ids = $(if $(lib),$(foreach id,$(testbench-test-ids),$(if $(filter $(lib),$(hostprog-$(id)-owner)),$(id))),$(testbench-test-ids))
selected-host-bench-ids = $(if $(lib),$(foreach id,$(testbench-bench-ids),$(if $(filter $(lib),$(hostprog-$(id)-owner)),$(id))),$(testbench-bench-ids))
selected-host-example-ids = $(if $(lib),$(foreach id,$(testbench-example-ids),$(if $(filter $(lib),$(hostprog-$(id)-owner)),$(id))),$(testbench-example-ids))
selected-host-header-check-ids = $(foreach id,$(header-check-ids),$(if $(if $(lib),$(filter $(lib),$(headercheck-$(id)-owner)),y),$(id)))
selected-freestanding-header-check-ids = $(foreach id,$(header-check-ids),$(if $(if $(lib),$(filter $(lib),$(headercheck-$(id)-owner)),y),$(id)))
selected-freestanding-check-ids = $(foreach id,$(freestanding-check-ids),$(if $(filter y,$(library-$(freestandingcheck-$(id)-owner)-is-supported)),$(if $(if $(lib),$(filter $(lib),$(freestandingcheck-$(id)-owner)),y),$(id))))

.PHONY: _host-test _example _host-example _bench _host-bench _host-header-check
.PHONY: _freestanding-header-check _freestanding-check
.PHONY: _build-host-tools _build-host-tool _run-host-tool
_build-host-tools: _build-host-libs $(addprefix host-tool-,$(host-tool-ids))
	$(q)$(echo) "All Host tools built"

_build-host-tool: _build-host-libs host-tool-$(tool)
	$(q)$(echo) "Host tool $(tool) built"

_run-host-tool: _build-host-libs run-host-tool-$(tool)

_host-test: _build-host-libs
	$(q)$(s-run-testbenches) root=$(call shq,$(path-e)) kind=test mode=$(mode) \
		sanitize=$(call shq,$(sanitize)) lib=$(call shq,$(lib)) \
		host-features=$(call shq,$(host-features)) make-command=$(call shq,$(MAKE)) q=$(call shq,$(q))

_example: _build-host-libs $(addprefix host-program-,$(selected-host-example-ids))
	$(q)$(echo) "Host examples built"

_host-example: _build-host-libs
	$(q)$(s-run-testbenches) root=$(call shq,$(path-e)) kind=example mode=$(mode) \
		sanitize=$(call shq,$(sanitize)) lib=$(call shq,$(lib)) \
		host-features=$(call shq,$(host-features)) make-command=$(call shq,$(MAKE)) q=$(call shq,$(q))

_bench: _build-host-libs $(addprefix host-program-,$(selected-host-bench-ids))
	$(q)$(echo) "Host benchmarks built"

_host-bench: _build-host-libs
	$(q)$(s-run-testbenches) root=$(call shq,$(path-e)) kind=bench mode=$(mode) \
		sanitize=$(call shq,$(sanitize)) lib=$(call shq,$(lib)) \
		host-features=$(call shq,$(host-features)) make-command=$(call shq,$(MAKE)) q=$(call shq,$(q))

.NOTPARALLEL: _host-example _host-bench

_host-header-check: $(addprefix host-header-check-,$(selected-host-header-check-ids))
	$(q)$(echo) "Host header checks passed"

_freestanding-header-check: $(addprefix freestanding-header-check-,$(selected-freestanding-header-check-ids))
	$(q)$(echo) "Freestanding header checks passed for $(arch)"

_freestanding-check: $(addprefix freestanding-check-,$(selected-freestanding-check-ids))
	$(q)$(echo) "Freestanding checks passed for $(arch)"

.PHONY: build-hosttool build-host-libs build-host-lib build-host-tools build-host-tool run-host-tool
.PHONY: host-test example host-example bench host-bench
.PHONY: host-header-check freestanding-header-check freestanding-check
.PHONY: check-lib build-lib-matrix
build-host-libs:
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _build-host-libs

build-host-lib: _require-host-lib
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) host-build-lib-$(lib)

build-hosttool: $(host-tool-build-targets)
	$(q)$(echo) "All Host build tools built"

build-host-tools: build-hosttool

build-host-tool: _require-host-tool
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) tool=$(tool) _prepare-host-tool-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) tool=$(tool) _build-host-tool

run-host-tool: _require-host-tool
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) tool=$(tool) _prepare-host-tool-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) tool=$(tool) _run-host-tool

host-test:
	$(if $(lib),$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib))),)
	$(if $(lib),$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host)),)
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _host-test

example:
	$(if $(lib),$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib))),)
	$(if $(lib),$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host)),)
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _example

host-example:
	$(if $(lib),$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib))),)
	$(if $(lib),$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host)),)
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _host-example

bench:
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 mode=$(if $(filter command line,$(origin mode)),$(mode),release) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 mode=$(if $(filter command line,$(origin mode)),$(mode),release) sanitize=$(sanitize) _bench

host-bench:
	$(if $(lib),$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib))),)
	$(if $(lib),$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host)),)
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 mode=$(if $(filter command line,$(origin mode)),$(mode),release) sanitize=$(sanitize) lib=$(lib) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 mode=$(if $(filter command line,$(origin mode)),$(mode),release) sanitize=$(sanitize) lib=$(lib) _host-bench

host-header-check: _require-host-lib
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _host-header-check

freestanding-header-check: _require-lib
	$(if $(filter freestanding,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment freestanding))
	$(if $(filter $(arch),$(or $(library-$(lib)-support-archs),riscv64 loongarch64)),,$(error library $(lib) does not support architecture $(arch)))
	$(q)$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) lib=$(lib) _freestanding-header-check

freestanding-check:
	$(if $(lib),$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib))),)
	$(if $(lib),$(if $(filter freestanding,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment freestanding)),)
	$(if $(lib),$(if $(filter $(arch),$(or $(library-$(lib)-support-archs),riscv64 loongarch64)),,$(error library $(lib) does not support architecture $(arch))),)
	$(q)$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) lib=$(lib) _freestanding-check

check-lib: _require-lib
	$(if $(filter freestanding,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment freestanding))
	$(if $(filter $(arch),$(or $(library-$(lib)-support-archs),riscv64 loongarch64)),,$(error library $(lib) does not support architecture $(arch)))
	$(q)$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) build-lib-$(lib)
	$(q)$(if $(filter y,$(library-$(lib)-is-header-only)),$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) lib=$(lib) _freestanding-header-check,:)
	$(q)$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) lib=$(lib) _freestanding-check
	$(q)$(if $(filter host,$(library-$(lib)-support-environments-all)),$(MAKE) --no-print-directory allow-target-arch=1 lib=$(lib) mode=$(mode) host-test,:)

build-lib-matrix: _require-lib
	$(q)set -e; for matrix_arch in $(or $(library-$(lib)-support-archs),riscv64 loongarch64); do \
		$(MAKE) --no-print-directory arch=$$matrix_arch mode=$(mode) build-lib-$(lib); \
		$(if $(filter y,$(library-$(lib)-is-header-only)),$(MAKE) --no-print-directory arch=$$matrix_arch mode=$(mode) lib=$(lib) _freestanding-header-check;,) \
		$(MAKE) --no-print-directory arch=$$matrix_arch mode=$(mode) lib=$(lib) _freestanding-check; \
	done
	$(q)$(if $(filter host,$(library-$(lib)-support-environments-all)),$(MAKE) --no-print-directory allow-target-arch=1 lib=$(lib) mode=$(mode) build-host-lib,:)
