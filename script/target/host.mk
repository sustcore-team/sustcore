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

.PHONY: _require-lib _require-host-lib _prepare-host-deps
_require-lib:
	$(if $(strip $(lib)),,$(error missing required lib=<id>))
	$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib)))

_require-host-lib: _require-lib
	$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host))

_prepare-host-deps:
	$(q)set -e; $(foreach owner,$(library-ids),$(s-resolve-deps) root=$(path-e) owner=$(owner) environment=host arch=$(host-arch) output=$(path-deps)/host-$(owner).mk;)

selected-host-test-ids = $(if $(lib),$(foreach id,$(testbench-test-ids),$(if $(filter $(lib),$(hostprog-$(id)-owner)),$(id))),$(testbench-test-ids))
selected-host-bench-ids = $(if $(lib),$(foreach id,$(testbench-bench-ids),$(if $(filter $(lib),$(hostprog-$(id)-owner)),$(id))),$(testbench-bench-ids))
selected-host-header-check-ids = $(foreach id,$(header-check-ids),$(if $(if $(lib),$(filter $(lib),$(headercheck-$(id)-owner)),y),$(id)))
selected-freestanding-header-check-ids = $(foreach id,$(header-check-ids),$(if $(if $(lib),$(filter $(lib),$(headercheck-$(id)-owner)),y),$(id)))

.PHONY: _host-test _bench _host-bench _host-header-check _freestanding-header-check
_host-test: _build-host-libs
	$(q)$(s-run-testbenches) root=$(call shq,$(path-e)) kind=test mode=$(mode) \
		sanitize=$(call shq,$(sanitize)) lib=$(call shq,$(lib)) \
		host-features=$(call shq,$(host-features)) make-command=$(call shq,$(MAKE)) q=$(call shq,$(q))

_bench: _build-host-libs $(addprefix host-program-,$(selected-host-bench-ids))
	$(q)$(echo) "Host benchmarks built"

_host-bench: _build-host-libs
	$(q)$(s-run-testbenches) root=$(call shq,$(path-e)) kind=bench mode=$(mode) \
		sanitize=$(call shq,$(sanitize)) lib=$(call shq,$(lib)) \
		host-features=$(call shq,$(host-features)) make-command=$(call shq,$(MAKE)) q=$(call shq,$(q))

.NOTPARALLEL: _host-bench

_host-header-check: $(addprefix host-header-check-,$(selected-host-header-check-ids))
	$(q)$(echo) "Host header checks passed"

_freestanding-header-check: $(addprefix freestanding-header-check-,$(selected-freestanding-header-check-ids))
	$(q)$(echo) "Freestanding header checks passed for $(arch)"

.PHONY: build-host-libs build-host-lib host-test bench host-bench
.PHONY: host-header-check freestanding-header-check check-lib build-lib-matrix
build-host-libs:
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _build-host-libs

build-host-lib: _require-host-lib
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) host-build-lib-$(lib)

host-test:
	$(if $(lib),$(if $(filter $(lib),$(library-ids-all)),,$(error unknown library id: $(lib))),)
	$(if $(lib),$(if $(filter host,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment host)),)
	$(q)$(MAKE) --no-print-directory validate-host
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _prepare-host-deps
	$(q)$(MAKE) --no-print-directory MAKEOVERRIDES= host-context=1 allow-target-arch=$(allow-target-arch) mode=$(mode) sanitize=$(sanitize) lib=$(lib) _host-test

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

check-lib: _require-lib
	$(if $(filter freestanding,$(library-$(lib)-support-environments-all)),,$(error library $(lib) does not support environment freestanding))
	$(if $(filter $(arch),$(or $(library-$(lib)-support-archs),riscv64 loongarch64)),,$(error library $(lib) does not support architecture $(arch)))
	$(q)$(if $(filter y,$(library-$(lib)-is-header-only)),$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) lib=$(lib) _freestanding-header-check,$(MAKE) --no-print-directory arch=$(arch) mode=$(mode) build-lib-$(lib))
	$(q)$(if $(filter taycpplib,$(lib)),$(MAKE) --no-print-directory -f $(path-s)/host/panic-link.mk global-env=$(global-env) environment=freestanding arch=$(arch) mode=$(mode) q=$(q) library-owner=taycpplib deps-file=$(path-deps)/taycpplib.mk check, :)
	$(q)$(if $(filter host,$(library-$(lib)-support-environments-all)),$(MAKE) --no-print-directory allow-target-arch=1 lib=$(lib) mode=$(mode) host-test,:)

build-lib-matrix: _require-lib
	$(q)set -e; for matrix_arch in $(or $(library-$(lib)-support-archs),riscv64 loongarch64); do \
		$(MAKE) --no-print-directory arch=$$matrix_arch mode=$(mode) $(if $(filter y,$(library-$(lib)-is-header-only)),lib=$(lib) _freestanding-header-check,build-lib-$(lib)); \
	done
	$(q)$(if $(filter host,$(library-$(lib)-support-environments-all)),$(MAKE) --no-print-directory allow-target-arch=1 lib=$(lib) mode=$(mode) build-host-lib,:)
