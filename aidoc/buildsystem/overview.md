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

Key files:

- `script/env/global.mk`
- `script/env/buildpath.mk`
- `script/env/shell.mk`
- `script/env/q.mk`

### `script/toolchain`

Defines compiler, linker, archiver, and QEMU-facing tool variables:

- `c.mk`
- `cpp.mk`
- `ld.mk`
- `ar.mk`
- `qemu.mk`

### `script/rules`

Defines thin build rules only:

- `asm.mk`
- `c.mk`
- `cpp.mk`
- `ld.mk`
- `ar.mk`

These files consume resolved variables and do not decide target kind, source
discovery, or architecture selection.

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

These files assemble:

- local sources
- flags
- include paths
- output target path
- dependencies

## High-Level Flow

1. `make switch` stores `arch/mode`
2. `make configure` generates cache fragments from TOML and project metadata
3. top-level Make reads shared cache fragments
4. `build-libs` builds visible static libraries for the current architecture
5. `build-kernel` invokes `kernel/Makefile`
6. `runonly` / `dbgonly` launch QEMU

`make update [arch=<arch>] [mode=<mode>]` rebuilds the selected compilation
database through Bear. Command-line architecture and mode overrides select the
database to update without changing the values persisted by `make switch`.

## Current Architecture

The current known architectures are:

- `riscv64`
- `loongarch64`

Library visibility can vary by architecture through `support-archs`.
