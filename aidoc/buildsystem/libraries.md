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
version = "0.1.0-dev.1"
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
  - concrete SemVer 2.0 library release version
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

## Version Naming And Requirements

`metadata.toml` uses `version` for the concrete version provided by a library.
It must use a full SemVer 2.0 form without a `v` prefix, for example:

```toml
version = "0.1.0"
version = "0.2.0-rc.1"
version = "0.2.0-dev.3+git.abc1234"
```

`dependencies.toml` uses `version` as a version-range requirement rather than
the dependency's concrete version. Supported forms include:

- `*` for any version
- exact and partial versions such as `1.2.3`, `1.2`, and `1.x`
- comparator and conjunction ranges such as `>=1.2.0 <2.0.0`
- compatibility ranges such as `^0.2.0`, `~1.2`, and `1.2 - 2.0`
- logical OR, for example `^1.2 || ^2.0`

Build metadata is retained for traceability but ignored for version precedence
and range matching. Ranges exclude prerelease versions by default; a range
clause must explicitly contain a prerelease comparator for the same
`MAJOR.MINOR.PATCH` core version to match one.

## Registry Generation

The shared registry is built by:

- `script/py/libregistry.py`
- `script/py/build_libs.py`

Generated output:

- `script/.cache/libraries.mk`

This registry fragment contains:

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

Generated build-target output:

- `script/.cache/build-libs.mk`

This build-target fragment contains:

- `build-lib-<id>`
- `build-lib-targets-<arch>`
- `build-libs`

Each non-header-only target passes its matching
`build-header-lib-<id>.mk` file to the library sub-make. The header declares
the library owner and source root, with defaults for its object directory
(`$(path-obj)/libs/<id>`) and final archive
(`$(path-bin)/libs/<libname>`). Header-only libraries still receive a header
with an empty `target`.

## Component Build Fragments

Each buildable library or module uses three root fragments:

- `Makefile` loads the component header, flags, collection result, and rules.
- `flags.mk` declares component-local compiler flags and include paths.
- `collect.mk` invokes `script/build/collector.mk` for the component root.

Source selection belongs exclusively in `include.mk`. The collector reads the
root and all nested `include.mk` files, classifies their `src-y` and `src-n`
entries by language, and prefixes nested paths relative to the component root.

## Dependency File Schema

Dependency owners use:

```toml
[[dependencies]]
lib = "mini-cstd"


[[riscv64.dependencies]]
lib = "sbi"

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
