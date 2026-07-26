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
- `deps-<id>.mk`

These describe:

- global library registration
- generated `build-libs` targets
- module makefile and build-target indexes
- resolved target dependency sets

`make configure` also emits per-component headers, but does not include them
from either `config.mk` or the top-level Makefile:

- `build-header-lib-<id>.mk`
- `build-header-module-<id>.mk`
- `build-header-kernel.mk`

Each header sets `owner-id` and `owner-root`, and defaults `obj-root` and
`target` with `?=`. Build indexes pass the matching header to the component
sub-make, keeping those generic variables scoped to one component.

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
- module build indexes
- component build headers
- owner dependency fragments

## Why This Split Exists

Project-level dependency state should not be silently injected into every
sub-make through `global.mk`.

The split keeps:

- `global.mk` focused on environment and build-system defaults
- top-level / target-level Makefiles responsible for project dependency data
