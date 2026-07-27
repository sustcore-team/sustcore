# Configuration Pipeline

## Two Configuration Classes

The current system splits generated Make fragments into two classes.

## Build-system Configuration

These fragments are included through `script/.cache/config.mk` and then loaded
from `script/env/global.mk`.

Current members:

- `clang.mk`
- `path.mk`
- `qemu.mk`
- `kernel.mk`

These describe:

- toolchain choice
- output directory layout
- QEMU runtime parameters
- kernel boot-mode defaults

## Project Configuration

These fragments are **not** included through `config.mk`.
They are included explicitly by top-level or target-local Makefiles.

Current members:

- `libraries.mk`
- `build-libs.mk`
- `programs.mk`
- `testbench.mk`
- `deps/<id>.mk`

These describe:

- global library registration
- generated `build-libs` targets
- module makefile and build-target indexes
- resolved target dependency sets
- host test, benchmark, and header-check dispatch

`make configure` also emits per-component build contexts, but does not include them
from either `config.mk` or the top-level Makefile:

- `ctx/lib-<id>.mk`
- `ctx/module-<id>.mk`
- `ctx/hostprog-<id>.mk`
- `ctx/kernel.mk`

Each context sets `owner-id` and `owner-root`, and defaults `obj-root` and
`target` with `?=`. Build indexes pass the matching context through `ctx=` to the component
sub-make, keeping those generic variables scoped to one component.

`script/env/global.mk` exposes the generated roots as:

```make
path-deps := $(path-cache)/deps
path-ctx  := $(path-cache)/ctx
```

Host validation additionally emits `host.mk`. This fragment is not included
through `config.mk`; only dedicated host sub-makes load it after
`make validate-host` succeeds.
Host commands then atomically emit `deps/host-<id>.mk` for host-visible owners.

## `make switch`

`make switch arch=<arch> mode=<mode>` persists build selection only in:

- `script/.cache/.switch.mk`

It does not regenerate library metadata or dependency caches. After persisting
the build selection, the target also refreshes the ignored clangd compilation
database copy at `build/compile_commands.json`.

## `make configure`

`make configure config=<name>` reads:

- `config/<name>/*.toml`

It generates:

- build-system configuration fragments
- library registry fragments
- module build indexes
- component build contexts
- owner dependency fragments

All known freestanding architectures are resolved in one configuration pass.
Changing the persisted architecture or mode therefore does not regenerate
dependency fragments. Legacy `arch=` and `mode=` arguments are accepted with a
warning but do not affect configuration output or `.switch.mk`.

Configuration generation only checks the shape of `[host]`. It does not run
host probes, so freestanding configuration remains usable on machines without
a host toolchain.

## Host Toolchain Configuration

`clang.toml` accepts the following native toolchain configuration:

```toml
[host]
clang = "clang"
"clang++" = "clang++"
ar = "llvm-ar"
sysroot = "/"
cppstdlib = "auto"
cflags = []
cxxflags = []
ldflags = []
```

`sysroot` is mandatory. `cppstdlib` accepts `auto`, `libstdc++`, or `libc++`.
The flags are arrays in TOML and are kept separate for C compilation, C++
compilation, and compiler-driver linking.

`make validate-host [host-arch=<arch>]` verifies compiler family, C/C++ target
agreement, native architecture, LLVM ar, system include search, C++ standard
library provider, and C/C++ compile-link-run probes. It writes `host.mk` only
after all checks pass. `arch=` is rejected for this target; `host-arch=` is an
optional assertion against the detected architecture.

The configuration can be explicitly overridden with `HOST_CLANG`,
`HOST_CLANGXX`, `HOST_LLVM_AR`, `HOST_SYSROOT`, `HOST_CPPSTDLIB`,
`HOST_CFLAGS`, `HOST_CXXFLAGS`, and `HOST_LDFLAGS`. Flag overrides use shell
token syntax and replace the configured array.

## Compilation Databases

`make update [arch=<arch>] [mode=<mode>]` runs the active `build-kernel` flow
through Bear and writes:

```text
build/<mode>/<arch>/compile_commands.json
```

When `arch` or `mode` is omitted, the value persisted by `make switch` is used.
Explicit overrides update another database without changing the current build
selection.

## Build Dimension Selectors

After a target buildpath has selected the environment, architecture, and mode,
the build exports `y`/`n` selectors including:

```text
is-host                 is-freestanding
is-riscv64              is-loongarch64
is-debug                is-release
is-riscv64-debug        is-riscv64-release
is-loongarch64-debug    is-loongarch64-release
is-freestanding-riscv64 is-freestanding-loongarch64
is-host-<native-arch>
```

Generated Make fragments append matching values through computed names such as
`owner-dep-ids-$(is-riscv64)`,
`library-ids-$(is-freestanding-riscv64)`, and
`testbench-program-ids-$(is-host)`, then publish only the corresponding `*-y`
bucket. Exactly one environment, architecture, mode, and active
architecture-mode pair is selected.

## Cache Lifetime

- `switch` changes only `.switch.mk` and the stable clangd selection.
- `configure` regenerates configuration, registries, component contexts, and all
  freestanding dependency variants; successful regeneration removes stale host
  validation and dependency fragments.
- `clean` removes the complete configured build root and preserves configuration.
- `cleandist` performs `clean`, removes every generated cache entry, and preserves
  only `.switch.mk`.

Build entry points diagnose a missing or incomplete configuration and require a
new `make configure` after `cleandist`.

`make update-host [mode=<mode>] [sanitize=<set>]` validates the native
toolchain, resolves host dependencies, and captures all host library, test, and
benchmark translation units without running testbench executables. It writes:

```text
build/<mode>/host/<host-triple>/compile_commands.json
build/<mode>/host/<host-triple>/sanitize/<profile>/compile_commands.json
```

Bear writes a temporary file first. The build publishes it only after the JSON
array validates, so a failed capture does not replace a previous database.

Both `make switch` and `make configure` atomically copy the selected database to
the stable clangd entry after writing their cache state:

```text
build/compile_commands.json
```

If the selected database has not been generated, the old stable copy is removed
to prevent clangd from using flags for another architecture. clangd can use the
stable directory through `--compile-commands-dir=build`.

Use `make clangd-host [mode=<mode>] [sanitize=<set>]` to select a generated
host database and `make clangd-target [arch=<arch>] [mode=<mode>]` to select a
freestanding database. These targets atomically update only the stable copy;
they do not change `.switch.mk`. `make switch` and `make configure` continue to
select the persisted freestanding variant.

## Why This Split Exists

Project-level dependency state should not be silently injected into every
sub-make through `global.mk`.

The split keeps:

- `global.mk` focused on environment and build-system defaults
- top-level / target-level Makefiles responsible for project dependency data
