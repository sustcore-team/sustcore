# Sustcore Current Status

## Overview

This repository is in the middle of a build system and kernel tree refactor.
The active build system is the new `script/`-based Make/TOML pipeline, not the
legacy `.vscode/sustcore` implementation. The legacy tree is still useful as a
reference, but it does not describe the current live behavior.

## Current Build Entry Points

- `make init`
  - Creates required cache directories and prepares Python helper scripts.
- `make switch arch=<arch> mode=<mode>`
  - Updates `script/.cache/.switch.mk`.
  - Its job is only to persist the selected architecture and mode.
- `make configure config=<name>`
  - Reads `config/<name>/*.toml`.
  - Generates `.cache/*.mk` and all freestanding architecture dependencies.
  - Legacy `arch=` and `mode=` arguments are ignored with a warning.
- `make validate-host [host-arch=<arch>]`
  - Validates the configured native Clang/Clang++/LLVM ar toolchain.
  - Generates `script/.cache/host.mk` without changing `.switch.mk`.
- `make build-libs`
  - Builds the currently visible libraries for the selected architecture.
- `make build-kernel`
  - Builds the kernel through `kernel/Makefile`.
- `make build-host-libs` / `make build-host-lib lib=<id>`
  - Builds validated native archives or header checks.
- `make host-test [lib=<id>]` / `make host-bench [lib=<id>]`
  - Builds and runs registered native tests or benchmarks.
- `make bench`
  - Builds all registered native benchmarks in release mode without running them.
- `make check-lib lib=<id>` / `make build-lib-matrix lib=<id>`
  - Checks one variant or every supported freestanding/native variant.
- `make runonly`
  - Runs QEMU without rebuilding the kernel.
- `make dbgonly`
  - Runs QEMU in debug mode (`-s -S`) without rebuilding the kernel.

## Current Cache Model

There are now two categories of generated Make fragments under `script/.cache/`.

### Build-system configuration

Loaded through `script/.cache/config.mk`:

- `clang.mk`
- `path.mk`
- `qemu.mk`
- `kernel.mk`

These files describe compiler selection, output directory layout, QEMU
settings, and kernel boot-mode configuration.

### Project configuration

Loaded explicitly by top-level or target-local Makefiles:

- `libraries.mk`
- `build-libs.mk`
- `deps/*.mk`
- `ctx/*.mk`

These files describe the global library registry, generated library build
targets, and resolved dependencies for the kernel.

The registry keeps `library-ids-all` only for cross-environment validation and
matrix enumeration. Active library lists, architecture-specific CRT/linker
fields, build targets, host programs, and header checks are selected through
`is-host`, `is-freestanding`, `is-<arch>`, and combined environment/architecture
selectors, then consumed from their `*-y` buckets.

Freestanding dependency fragments contain every known target architecture and
select the active values through `is-<arch>` variables. They are not regenerated
by `make switch`.

## Current Library System

Libraries are registered with `metadata.toml`.
The active schema is:

```toml
[[libmeta]]
id = "example"
libname = "libexample.a"
makefile = "Makefile"
target = "build-static"
version = "0.1.0-dev.1"
support-archs = ["riscv64"]

include-c = ["include"]
include-cpp = ["include"]
include-asm = ["include"]
```

Important current rules:

- A single `metadata.toml` may contain multiple `[[libmeta]]` entries.
- `id` must still be globally unique.
- `libname` is the generated static archive name; an empty value denotes a
  header-only library.
- `support-archs` is an allow-list.
- `build-libs` is generated from library metadata and skips header-only
  libraries.

## Current Dependency System

The kernel currently uses `kernel/dependencies.toml`.
The active shape is:

```toml
[[dependencies]]
lib = "mini-cstd"


[[riscv64.dependencies]]
lib = "sbi"

```

The resolver currently:

- validates library versions as complete SemVer 2.0 versions
- supports exact, wildcard, and partial version expressions
- supports comparator expressions, range conjunctions, and logical OR
- supports caret, tilde, and hyphen ranges
- follows npm-style prerelease range matching
- ignores build metadata when comparing versions

It does **not** yet support:

- multiple versions under the same `id`

Version naming rules:

- `metadata.toml` uses a concrete library release version such as `0.1.0`,
  `0.2.0-rc.1`, or `0.2.0-dev.3+git.abc1234`.
- `dependencies.toml` uses a version range: `*` accepts any version, while
  `^0.1.0`, `~0.1`, and `1.2 - 2.0` express compatibility ranges.
- Build metadata is retained for traceability but cannot pin a dependency to a
  specific build.

## Current Kernel Build Status

The kernel Makefile stack is now split into:

- `kernel/Makefile`
- `kernel/flags.mk`
- `kernel/collect.mk`
- `kernel/include.mk`
- `kernel/enable.mk`
- `kernel/variant.riscv64.mk`
- `kernel/variant.loongarch64.mk`
- `kernel/dependencies.toml`

Current status:

- object compilation is wired through `script/rules/*.mk`
- static libraries are built through per-library Makefiles and `llvm-ar`
- the kernel links against resolved libraries from `deps/kernel.mk`
- `kernel-path` is controlled by the top-level Makefile and passed to the
  kernel sub-make

## Current Known Limitations

- `build-libs` relies on second expansion and still needs careful validation
  around architecture switching behavior.
- the kernel tree is still incomplete compared with the legacy repository
- the C/C++ runtime split is still evolving
- host libraries, tests, header checks, benchmarks, sanitizers, and library
  matrices are available through the dedicated host/check targets

## Current Host System

- Host configuration lives under `[host]` in `clang.toml`.
- `host.sysroot` is mandatory and is used for every compile/link probe.
- Only native Clang, Clang++, and LLVM ar configurations are accepted.
- Host architecture comes from the compiler triple and `uname`, never from
  the target architecture cached by `make switch`.
- Validated host paths are rooted at
  `build/<mode>/host/<host-triple>/`.
- Host dependencies are resolved after validation from the public,
  environment, and native-architecture sections.
- Sanitizer profiles use isolated subdirectories below the host triple.
- C++ compilation receives exactly one of `TAY_ENV_HOST=1` and
  `TAY_ENV_FREESTANDING=1` from the selected toolchain environment.

## Reference Material

Use the new documentation under `aidoc/buildsystem/` for the active system.
Use `.vscode/sustcore/` only as historical/reference material.
