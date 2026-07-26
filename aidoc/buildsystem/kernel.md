# Kernel Build Flow

## Files

The kernel build is currently split into:

- `kernel/Makefile`
- `kernel/flags.mk`
- `kernel/collect.mk`
- `kernel/include.mk`
- `kernel/enable.mk`
- `kernel/variant.riscv64.mk`
- `kernel/variant.loongarch64.mk`
- `kernel/dependencies.toml`

## Kernel Entry

The top-level Makefile invokes:

```make
$(MAKE) -f $(path-e)/kernel/Makefile \
    global-env=$(global-env) \
    arch=$(arch) \
    q=$(q) \
    build-header=$(path-cache)/build-header-kernel.mk \
    kernel-path=$(kernel-path) \
    build
```

`kernel/Makefile` is a passive sub-build entry.

## Output Path

The top-level Makefile owns:

```make
kernel-path ?= $(path-bin)/kernel/sustcore.bin
```

`make configure` emits `build-header-kernel.mk` for the fixed `kernel` owner.
The kernel sub-make consumes its root, object directory, and target from that
header:

```make
owner-id := kernel
owner-root := /.../kernel
obj-root ?= $(path-obj)/kernel
target ?= $(kernel-path)
```

## Source Collection

Kernel source discovery currently uses:

- `script/build/collector.mk`
- `kernel/collect.mk`
- root and subdirectory `include.mk`

`collect.mk` only invokes the shared collector. Each `include.mk` declares the
sources in its own directory through `src-y` and `src-n`; this includes the
kernel root when it owns sources directly.

Current active source model:

- `src-y`
- `src-n`
- expanded into:
  - `sources-y-asm`
  - `sources-y-c`
  - `sources-y-cpp`

## Boot Selection

Boot mode comes from:

- `script/.cache/kernel.mk`
  - `<arch>-boot := sbi|laboot`
- `kernel/enable.mk`

Current rules:

- `riscv64` defaults to `sbi`
- `loongarch64` defaults to `laboot`
- command-line `enable-sbi` / `enable-laboot` can override

## Injected Dependencies

The kernel no longer hardcodes most library dependencies.
Instead it consumes:

- `deps-kernel.mk`

Current injection points in `kernel/Makefile`:

- `archives += $(or $(kernel-dep-archives-$(arch)),$(kernel-dep-archives))`
- `includes-c += ...`
- `includes-cpp += ...`
- `includes-asm += ...`

## Current Status

The kernel build currently supports:

- object compilation
- dependency file generation
- link invocation
- `kernel-path` override
- boot-specific link script selection

The kernel is still under refactor, so full runtime/link completeness is still
evolving.
