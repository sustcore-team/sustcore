# 关于 qemu 模拟时 vectored 模式的 stvec 行为异常的报告

## 1. `stvec` 的正常行为

`stvec` 的结构为

 | bits     | function |
 | -------- | -------- |
 | `[0:1]`  | MODE     |
 | `[2:63]` | BASE     |

当 `MODE = 0(direct)` 时, 中断发生时 `pc = BASE << 2`. 或者说 `pc = stvec & ~0x11`.
当 `MODE = 1(vectored)`时, 如果是异步中断发生, 则`pc = BASE << 2 + cause * 4`.

在最新版的`qemu`中, 其表现为

```c
env->pc = (env->stvec >> 2 << 2) +
                  ((async && (env->stvec & 3) == 1) ? cause * 4 : 0);
```

## 2. 针对 `vectored` 模式下 `stvec` 所对应的中断向量表的设计

其中每个4字节条目均可形如`j xxx_handler`

通过 `emit_j_ins` 与 `emit_ivt_entry`, 可以自动生成这样的指令, 加入到`dword`数组`IVT`中.

## 3. 故障发现

再按照上述设计编写程序, 并启用`vectored`模式后, 发现无法正常进入ISR.

## 4. 原因探究

通过对程序行为的跟踪与对`pc`寄存器位置的分析, 发现`pc`寄存器的值有时会等于`stvec`.
使用`gdb`在`*stvec`处下断点`b *$stvec`, 发现程序确实运行至了`*stvec`.

## 5. 原因分析

查看`qemu`源码, 发现在2019年之前的`qemu`, 对于`stvec`的处理为
```c
en
```

由于使用的`qemu`为从`ubuntu`官方源下载的, 猜测错误在于该版本的`qemu`还未使用正确的处理逻辑.

`qemu-system-riscv64 --version`输出为:

```
QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.10)
Copyright (c) 2003-2023 Fabrice Bellard and the QEMU Project developers
```

## 6. 残留疑点

该部分处理逻辑在2019年已被更新. 而我们使用的`qemu`版本为8.2.0, 依理论应已修复。