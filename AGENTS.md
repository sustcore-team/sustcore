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
- `make configure config=<name> [arch=<arch>]`
  - Reads `config/<name>/*.toml`.
  - Generates `.cache/*.mk`.
- `make build-libs`
  - Builds the currently visible libraries for the selected architecture.
- `make build-kernel`
  - Builds the kernel through `kernel/Makefile`.
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
- `deps-kernel.mk`

These files describe the global library registry, generated library build
targets, and resolved dependencies for the kernel.

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
- `mini-cstd` currently exports no include paths.
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
- the kernel links against resolved libraries from `deps-kernel.mk`
- `kernel-path` is controlled by the top-level Makefile and passed to the
  kernel sub-make

## Current Known Limitations

- `build-libs` relies on second expansion and still needs careful validation
  around architecture switching behavior.
- the kernel tree is still incomplete compared with the legacy repository
- the C/C++ runtime split is still evolving

## Reference Material

Use the new documentation under `aidoc/buildsystem/` for the active system.
Use `.vscode/sustcore/` only as historical/reference material.
