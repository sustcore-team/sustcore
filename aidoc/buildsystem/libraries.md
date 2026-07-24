# Library Registry And Dependency Resolution

## Metadata Schema

Libraries are registered through `metadata.toml`.
The current schema uses `[[libmeta]]`.

Example:

```toml
[[libmeta]]
id = "sbi"
libname = "libsbi.a"
makefile = "Makefile"
target = "build-static"
version = "0.0.1"
support-archs = ["riscv64"]

include-c = ["include"]
include-cpp = ["include"]
include-asm = ["include"]
```

## Current Field Meanings

- `id`
  - global logical identifier
- `libname`
  - actual archive file name; empty string means header-only
- `makefile`
  - path relative to the metadata file
- `target`
  - sub-make target used to build that library
- `version`
  - semver-like version string
- `support-archs`
  - allow-list of supported architectures
- `include-c/cpp/asm`
  - exported include roots for each language

## Current Rules

- `id` must be globally unique across all dependency owners
- one metadata file may contain multiple `[[libmeta]]`
- missing `support-archs` means “available on all architectures”
- missing `include-*` means “export nothing”
- `libname = ""` means “header-only”

## Registry Generation

The shared registry is built by:

- `script/py/libregistry.py`
- `script/py/build_libs.py`

Generated output:

- `script/.cache/libraries.mk`

This file currently contains:

- `library-ids`
- `library-ids-riscv64`
- `library-ids-loongarch64`
- `library-<id>-version`
- `library-<id>-libname`
- `library-<id>-makefile`
- `library-<id>-target`
- `library-<id>-archive`
- `library-<id>-is-header-only`
- `library-<id>-include-c`
- `library-<id>-include-cpp`
- `library-<id>-include-asm`
- `build-lib-<id>`
- `build-libs`

## Dependency File Schema

Dependency owners use:

```toml
[[dependencies]]
lib = "mini-cstd"
version = "*.*.*"

[[riscv64.dependencies]]
lib = "sbi"
version = "*.*.*"
```

The resolver:

- reads top-level `[[dependencies]]`
- reads arch-specific `[[<arch>.dependencies]]`
- matches them against the current registry
- validates that all transitive dependencies are explicitly listed by the owner
- reuses the child dependency version expression in missing-dependency diagnostics

Generated output:

- `script/.cache/deps-<id>.mk`

This file exports:

- `<id>-dep-ids`
- `<id>-dep-ids-<arch>`
- `<id>-dep-archives`
- `<id>-dep-archives-<arch>`
- `<id>-includes-c`
- `<id>-includes-cpp`
- `<id>-includes-asm`
- `<id>-includes-c-<arch>`
- `<id>-includes-cpp-<arch>`
- `<id>-includes-asm-<arch>`

## Current Examples

- `mini-cstd`
  - architecture-neutral
  - exports no include paths
- `fdt`
  - architecture-neutral
  - exports libfdt headers
- `sbi`
  - `support-archs = ["riscv64"]`
  - only visible to `riscv64`

## Current Limitations

- repeated `id` values are not supported
- multiple versions under the same `id` are not supported
- library-local `dependencies.toml` is shared by all `[[libmeta]]` entries in the same directory
