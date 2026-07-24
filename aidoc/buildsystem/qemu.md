# QEMU Integration

## Configuration Source

QEMU settings are read from:

- `config/<name>/qemu.toml`

and converted into:

- `script/.cache/qemu.mk`

## Generated Variables

Current generated variables include:

- `<arch>-qemu`
- `<arch>-qemu-generated-args`
- `<arch>-qemu-attached-args`

Examples:

- `riscv64-qemu`
- `loongarch64-qemu-generated-args`

## Generated Argument Sources

`qemu.py` currently handles:

- `qemu`
- `name`
- `memory`
- `rtc`
- `drives`
- `attached`
- `qemu_log`

### `qemu_log`

Example:

```toml
[riscv64.qemu_log]
file = "qemu.log"
type = ["guest_errors", "int"]
trace = ["virtio_blk_*"]
```

Generates:

- `-D qemu.log`
- `-d guest_errors,int,trace:virtio_blk_*`

## Top-Level Run Targets

Current top-level run targets are:

- `make runonly`
- `make dbgonly`

These are defined in:

- `script/target/run.mk`

## Hardcoded Runtime Policy

`run.mk` currently hardcodes:

- `-machine virt`
- `-nographic`
- `-bios default` for `riscv64`

It also injects:

- `-kernel $(kernel-path)`
- `-s -S` for `dbgonly`

## Current Command Model

The run command is built from:

- `qemu := $($(arch)-qemu)`
- `qemu-generated-args`
- `qemu-attached-args`
- `kernel-path`
- fixed runtime flags from `run.mk`

This keeps QEMU configuration split between:

- static policy in `run.mk`
- user/configurable policy in `qemu.toml`
