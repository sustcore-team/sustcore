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
- `deps-<id>.mk`

These describe:

- registered libraries
- `build-libs`
- resolved target dependency sets

## `make switch`

`make switch arch=<arch> mode=<mode>` only writes:

- `script/.cache/.switch.mk`

It should not regenerate library metadata or dependency caches.

## `make configure`

`make configure config=<name> [arch=<arch>]` reads:

- `config/<name>/*.toml`

It generates:

- build-system configuration fragments
- library registry fragments
- owner dependency fragments

## Why This Split Exists

Project-level dependency state should not be silently injected into every
sub-make through `global.mk`.

The split keeps:

- `global.mk` focused on environment and build-system defaults
- top-level / target-level Makefiles responsible for project dependency data
