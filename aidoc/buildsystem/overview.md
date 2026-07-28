# Build System Overview

## Goal

The current build system is a Make-driven pipeline with TOML-based
configuration, generated cache fragments, explicit per-component Makefiles, and
thin rule files for compilation and linking.

The system is intentionally being rebuilt in layers instead of reviving the
older monolithic `.vscode/sustcore` build frontend.

## Main Layers

### `script/env`

Defines the shared environment:

- workspace paths
- cache path
- shell helper variables
- selected `arch` / `mode`
- explicit `freestanding` / `host` build environment

Key files:

- `script/env/global.mk`
- `script/env/buildpath.mk`
- `script/env/host-buildpath.mk`
- `script/env/shell.mk`
- `script/env/q.mk`

### `script/toolchain`

Defines compiler, linker, archiver, and QEMU-facing tool variables:

- `c.mk`
- `cpp.mk`
- `ld.mk`
- `ar.mk`
- `qemu.mk`

The C/C++/link/archive fragments are shared by Host and freestanding builds;
validated Host values come from `script/.cache/host.mk`.

### `script/rules`

Defines thin build rules only:

- `asm.mk`
- `c.mk`
- `cpp.mk`
- `ld.mk`
- `ar.mk`

These files consume resolved variables and do not decide target kind, source
discovery, or architecture selection.

### `script/build`

Defines the shared component layers:

- `collector.mk` discovers sources declared by component `include.mk` files.
- `component.mk` selects the build environment, loads the generated context
  and dependencies, normalizes sources/objects, and installs compilation rules.
- `static-library.mk` adds the archiver toolchain and archive rule on top of
  `component.mk`.

`component.mk` deliberately stops at object generation. Kernel images, modules,
Host programs, and static archives retain separate final-artifact layers.

### `script/py`

Contains generators and parsers:

- config emitters
- library registry scanner
- dependency resolver
- semver matcher

### target-local Makefiles

Examples:

- `kernel/Makefile`
- `libs/sbi/Makefile`
- `libs/mincstd/Makefile`
- `third_party/libs/libfdt/Makefile`

Static-library Makefiles now only identify their component root and include
`script/build/static-library.mk`. Kernel, module, and Host-program layers include
`component.mk` and add only their own link or packaging semantics.

## High-Level Flow

1. `make switch` stores `arch/mode`
2. `make configure` generates cache fragments and all freestanding architecture
   dependencies from TOML and project metadata
3. top-level Make reads shared cache fragments
4. `build-libs` builds visible static libraries for the current architecture
5. `build-kernel` invokes `kernel/Makefile`
6. `build-host-libs`, `host-test`, and `bench` use the validated native toolchain
7. `update-host` captures native library and testbench compile commands
8. `runonly` / `dbgonly` launch QEMU

The host foundation is deliberately separate from this target flow.
`make validate-host [host-arch=<arch>]` validates the configured native
Clang toolchain and emits `script/.cache/host.mk`; it does not read or update
the architecture selected by `make switch`.

`make update [arch=<arch>] [mode=<mode>]` rebuilds the selected compilation
database through Bear. Command-line architecture and mode overrides select the
database to update without changing the values persisted by `make switch`.
`make update-host` generates the corresponding native database without running
tests or benchmarks. `clangd-host` and `clangd-target` switch the stable
`build/compile_commands.json` copy between those environments.

## Current Architecture

The current known architectures are:

- `riscv64`
- `loongarch64`

Library visibility can vary by architecture through `support-archs`.
Changing `arch` or `mode` with `make switch` selects from the generated cache;
it does not rerun dependency resolution.

Host builds use three independent dimensions:

- `environment=host`
- `arch` detected from the native compiler and checked against `uname`
- `mode=debug|release`

Their output root is `build/<mode>/host/<host-triple>/`. Sanitizer builds add a
separate `sanitize/<profile>/` subtree. Host libraries, tests, header checks,
and benchmarks keep independent archive, object, test, and benchmark outputs.
