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
- `deps-kernel.mk`

These files describe registered libraries, build targets, and resolved
dependencies for the kernel.

## Current Library System

Libraries are registered with `metadata.toml`.
The active schema is:

```toml
[[libmeta]]
id = "example"
makefile = "Makefile"
target = "build-static"
version = "0.0.1"
support-archs = ["riscv64"]

include-c = ["include"]
include-cpp = ["include"]
include-asm = ["include"]
```

Important current rules:

- A single `metadata.toml` may contain multiple `[[libmeta]]` entries.
- `id` must still be globally unique.
- `support-archs` is an allow-list.
- `mini-cstd` currently exports no include paths.
- `build-libs` is generated from library metadata.

## Current Dependency System

The kernel currently uses `kernel/dependencies.toml`.
The active shape is:

```toml
[[dependencies]]
lib = "mini-cstd"
version = "*.*.*"

[[riscv64.dependencies]]
lib = "sbi"
version = "*.*.*"
```

The resolver currently:

- supports exact versions
- supports semver wildcard expressions
- supports comparator expressions
- supports range conjunctions
- supports logical OR

It does **not** yet support:

- prerelease matching
- build metadata matching
- caret ranges
- tilde ranges
- multiple versions under the same `id`

## Current Kernel Build Status

The kernel Makefile stack is now split into:

- `kernel/Makefile`
- `kernel/flags.mk`
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

- `libraries.mk` currently contains both the global registry and the
  `build-libs` targets.
- `build-libs` relies on second expansion and still needs careful validation
  around architecture switching behavior.
- the kernel tree is still incomplete compared with the legacy repository
- the C/C++ runtime split is still evolving
- header-only library modeling is not implemented yet
- `libname` is still a design direction, not a completed part of the registry

## Reference Material

Use the new documentation under `aidoc/buildsystem/` for the active system.
Use `.vscode/sustcore/` only as historical/reference material.
