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
support-environments = ["freestanding", "host"]
testbench.test = ["testbench/test/metadata.toml"]
testbench.headercheck = ["testbench/headercheck/metadata.toml"]
testbench.bench = ["testbench/bench/metadata.toml"]

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
  - allow-list of supported freestanding architectures
- `support-environments`
  - allow-list containing `freestanding` and/or `host`; defaults to freestanding
- `include-c/cpp/asm`
  - exported include roots for each language
- `testbench.test/headercheck/bench`
  - explicit lists of testbench TOML files relative to this library metadata

## Current Rules

- `id` must be globally unique across all dependency owners
- one metadata file may contain multiple `[[libmeta]]`
- missing `support-archs` means “available on all architectures”
- `support-archs` never restricts native host visibility
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

- `library-ids-all` for cross-environment validation and matrix enumeration
- `library-ids-$(is-freestanding-<arch>)` and `library-ids-$(is-host)`
- `library-ids`, resolved from the active selector's `library-ids-y` bucket
- `library-<id>-version`
- `library-<id>-libname`
- `library-<id>-makefile`
- `library-<id>-target`
- `library-<id>-archive`
- `library-<id>-is-header-only`
- `library-<id>-include-c`
- `library-<id>-include-cpp`
- `library-<id>-include-asm`
- `library-<id>-crt0-$(is-<arch>)`, `crti`, `crtn`, and `ldscript` variants

Generated build-target output:

- `script/.cache/build-libs.mk`

This build-target fragment contains:

- `build-lib-<id>`
- `build-lib-targets-$(is-freestanding-<arch>)`
- `host-build-lib-targets-$(is-host)`
- `build-libs`

Each non-header-only target passes its matching
`$(path-ctx)/lib-<id>.mk` file through `ctx=` to the library sub-make. The context declares
the library owner and source root, with defaults for its object directory
(`$(path-obj)/libs/<id>`) and final archive
(`$(path-bin)/libs/<libname>`). Header-only libraries still receive a context
with an empty `target`.

## Component Build Fragments

Each buildable library or module uses three root fragments:

- `Makefile` loads the component context, flags, collection result, and rules.
- `flags.mk` declares component-local compiler flags and include paths.
- `collect.mk` invokes `script/build/collector.mk` for the component root.

Source selection belongs exclusively in `include.mk`. The collector reads the
root and all nested `include.mk` files, classifies their `src-y` and `src-n`
entries by language, and prefixes nested paths relative to the component root.

## Testbench Metadata

Each `[[libmeta]]` explicitly registers all of its testbench metadata files:

```toml
testbench.test = ["testbench/test/metadata.toml"]
testbench.headercheck = ["testbench/headercheck/metadata.toml"]
testbench.bench = ["testbench/bench/metadata.toml"]
```

When a `testbench` table is present, all three fields are required arrays;
unused categories use an empty array. Paths must name existing TOML files
relative to the library metadata. A metadata file may contain multiple
`[[libmeta]]` entries, each with independent testbench lists.

Test and benchmark files register executable programs:

```toml
[[hostprog]]
id = "example-test"
kind = "test"
makefile = "Makefile"
target = "build"
output = "example-test"
```

`kind = "test"` is valid only in a file listed by `testbench.test`, while
`kind = "bench"` is valid only in a file listed by `testbench.bench`.
Header-check files listed by `testbench.headercheck` contain only
`[[headercheck]]` entries. The scanner does not discover unregistered files by
directory or file name.

## Dependency File Schema

Dependency owners use:

```toml
[[dependencies]]
lib = "mini-cstd"


[[riscv64.dependencies]]
lib = "sbi"

[[host.dependencies]]
lib = "tayclib"

```

The resolver combines the public section, selected environment section, and
each applicable architecture section. A single `make configure` resolves and
validates every known freestanding architecture. Repeated dependencies are
deduplicated only when
their version expressions are identical; conflicting expressions are errors.
It then:

- reads top-level `[[dependencies]]`
- reads arch-specific `[[<arch>.dependencies]]`
- matches them against the current registry
- validates that all transitive dependencies are explicitly listed by the owner
- reuses the child dependency version expression in missing-dependency diagnostics

Generated output:

- `script/.cache/deps/<id>.mk`
- `script/.cache/deps/host-<id>.mk` after host validation

Each file uses the build dimension selectors internally and exports only the
selected final values:

- `<id>-dep-ids`
- `<id>-dep-archives`
- `<id>-includes-c`
- `<id>-includes-cpp`
- `<id>-includes-asm`

For example, public dependencies append to `<id>-dep-ids-y`, while an
architecture dependency appends through `<id>-dep-ids-$(is-<arch>)`; the final
`<id>-dep-ids` is assigned from the active `y` bucket. Host fragments use the
same interface but are generated only after native architecture validation.

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
