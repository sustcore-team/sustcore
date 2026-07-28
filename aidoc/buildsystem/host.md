# Host Builds, Tests, And Benchmarks

## Current Scope

Host commands validate a native Clang toolchain, resolve environment-specific
dependencies, and build isolated libraries, tests, header checks, and
benchmarks.

Run validation after selecting a configuration:

```text
make configure config=custom
make validate-host
make validate-host host-arch=x86_64
make build-host-libs
make host-test [lib=tayclib] [sanitize=address,undefined]
make bench
make host-bench [lib=taycpplib]
make update-host [mode=debug] [sanitize=address,undefined]
make clangd-host [mode=debug] [sanitize=address,undefined]
make clangd-target [arch=riscv64] [mode=debug]
```

Validation never invokes `make switch`, reads no cached target architecture,
and does not publish host configuration until every probe succeeds.

## Validation Contract

The C and C++ commands must both be Clang and must report the same target
triple. Its normalized architecture must match both `uname -m` and an optional
`host-arch` assertion. The archive command must report an LLVM version.

Every compiler probe uses the configured `--sysroot`. Verbose include search
may use that sysroot, Clang's resource directory, and an explicitly selected
GCC installation. Other system roots are rejected. C and C++ probes are then
linked with their matching compiler drivers and run as native executables.

For C++, `cppstdlib=libstdc++` selects `-stdlib=libstdc++`, while
`cppstdlib=libc++` selects `-stdlib=libc++`. `auto` records whichever provider
the standard-header probe detects. A GCC installation can be fixed through a
`--gcc-install-dir=...` entry in `cxxflags`.

## Generated State

Successful validation writes `script/.cache/host.mk` atomically. It records:

- normalized host architecture and full target triple
- resolved compiler-driver and archiver invocation paths
- compiler, archiver, and C++ standard library versions
- sysroot and effective C/C++/link flags
- a fingerprint covering all validated toolchain inputs
- optional features discovered by non-fatal compiler probes

Dedicated host sub-makes load `script/env/host-buildpath.mk`, which produces:

```text
build/<mode>/host/<host-triple>/bin/
build/<mode>/host/<host-triple>/obj/
build/<mode>/host/<host-triple>/test/
build/<mode>/host/<host-triple>/bench/
```

The freestanding build path remains `build/<mode>/<arch>/`. Shared C++ rules
force exactly one environment macro at the end of the compiler command:
`TAY_ENV_HOST=1` or `TAY_ENV_FREESTANDING=1`.

The static `script/toolchain/c.mk`, `cpp.mk`, `ar.mk`, and `ld.mk` fragments are
shared by both environments. `toolchain/environment.mk` resolves
`is-host`/`is-freestanding` to `y`/`n`; each toolchain fragment writes its two
candidates through those computed variable names and consumes only the
resulting `y-toolchain-*` value. Validated host values still come from the
generated `script/.cache/host.mk` fragment.

Sanitized builds use
`build/<mode>/host/<host-triple>/sanitize/<profile>/`; unsanitized paths remain
unchanged. Supported profiles are `address`, `undefined`, and
`address,undefined`.

## Command Semantics

- `build-host-libs` builds all host variants that declare an archive.
- `build-host-lib lib=<id>` builds the selected host archive, or runs host
  header checks when that specific host variant is header-only.
- `host-test` builds and runs every matching functionality test, including
  abort/stderr assertions. It continues after individual failures, reports
  `PASS`, `FAIL`, and `SKIP` for every selected program, then fails overall if
  any program failed.
- `bench` defaults to release and only builds every registered benchmark.
- `host-bench` defaults to release and sequentially runs every matching
  performance benchmark through the same aggregate runner.
- `host-header-check` compiles each applicable public header independently.
- `update-host` captures host libraries and all test/benchmark translation
  units through Bear without running the executables.
- `clangd-host` and `clangd-target` select the stable clangd database without
  changing the persisted target build selection.

Every host command validates first, resolves `deps/host-<owner>.mk` using the
validated native architecture, and then enters a recursive host sub-make.
These steps never update `.switch.mk`.

## Testbench Layout

Library testbenches use separate functionality and performance roots while
retaining the `kind = "test"|"bench"` metadata interface:

```text
libs/<library>/testbench/test/metadata.toml
libs/<library>/testbench/headercheck/metadata.toml
libs/<library>/testbench/bench/metadata.toml
```

The owning `[[libmeta]]` registers every file through the
`testbench.test`, `testbench.headercheck`, and `testbench.bench` lists. Test and
benchmark files contain matching `[[hostprog]]` entries; header-check files
contain only `[[headercheck]]`. Unregistered files are ignored and no legacy
directory scan is performed.

The aggregate Python runner handles executable testbenches only. Header checks
remain under their dedicated targets. The freestanding panic provider contract
is owned by `libs/taycpplib/Makefile` and is invoked through the normal
`build-lib-taycpplib`/`check-lib` path.
