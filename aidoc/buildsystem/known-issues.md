# Known Issues And Current Limits

## `build-libs` Second Expansion

`libraries.mk` contains the global registry, while `build-libs.mk` contains
the generated library build targets. Both are project configuration fragments
included explicitly by the top-level Makefile.

The `build-libs` target uses second expansion to select the right per-arch
build target set. This area should be watched carefully whenever the include
chain changes.

## Library Model Limits

Current limits:

- `id` must remain globally unique
- multiple versions under the same `id` are not supported
- a single library-local `dependencies.toml` is shared by all `[[libmeta]]` entries in the same directory

## Kernel Runtime Limits

The kernel build graph has been rebuilt, but runtime/link completeness is still
in progress.

Current weak spots include:

- C++ runtime integration
- basecpp integration
- final full-kernel source coverage

## Header Tree Refactor

The project is currently reorganizing C and C++ standard header ownership
between:

- `include/std`
- `third_party/include/std`
- `libs/mincstd`

Any work touching headers should assume this area is still evolving.
