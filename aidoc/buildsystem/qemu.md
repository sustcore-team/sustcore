# QEMU 集成

## 配置来源

QEMU 设置从以下文件读取：

- `config/<name>/qemu.toml`

并转换为：

- `script/.cache/qemu.mk`

## 生成变量

当前生成的变量包括：

- `<arch>-qemu`
- `<arch>-qemu-generated-args`
- `<arch>-qemu-attached-args`

例如：

- `riscv64-qemu`
- `loongarch64-qemu-generated-args`

## 参数来源

`qemu.py` 当前处理：

- `qemu`
- `name`
- `memory`
- `rtc`
- `drives`
- `attached`
- `qemu_log`

### `qemu_log`

示例：

```toml
[riscv64.qemu_log]
file = "qemu.log"
type = ["guest_errors", "int"]
trace = ["virtio_blk_*"]
```

生成：

- `-D qemu.log`
- `-d guest_errors,int,trace:virtio_blk_*`

## 顶层运行目标

当前顶层运行目标为：

- `make runonly`
- `make dbgonly`

定义位置：

- `script/target/run.mk`

## 硬编码的运行策略

`run.mk` 当前硬编码：

- `-machine virt`
- `-nographic`
- RISC-V64 使用 `-bios default`

它还会注入：

- `-kernel $(kernel-path)`
- `dbgonly` 使用 `-s -S`

## 当前命令模型

运行命令由以下部分组成：

- `qemu := $($(arch)-qemu)`
- `qemu-generated-args`
- `qemu-attached-args`
- `kernel-path`
- `run.mk` 中的固定运行 flags

因此，QEMU 配置被拆分为：

- `run.mk` 中的静态策略；
- `qemu.toml` 中由用户配置的策略。
