# 旧版本qemu中对stvec寄存器处理逻辑的错误

## 1. `stvec` 的正常行为

`stvec` 的结构为

 | bits     | function |
 | -------- | -------- |
 | `[0:1]`  | MODE     |
 | `[2:63]` | BASE     |

当 `MODE = 0(direct)` 时, 中断发生时 `pc = BASE << 2`. 或者说 `pc = stvec & ~0b11`.
当 `MODE = 1(vectored)`时, 如果是异步中断发生, 则`pc = BASE << 2 + cause * 4`.

在最新版的`qemu`中, 其表现为([target/riscv/cpu_helper.c#L2368](https://gitlab.com/qemu-project/qemu/-/blob/c4a9d49c7b23a02c646ebac756519c15a24f7ecc/target/riscv/cpu_helper.c#L2368))

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
/* handle the trap in S-mode */
/* No need to check STVEC for misaligned - lower 2 bits cannot be set */
env->pc = env->stvec;
```

是直接将`stvec`的值赋给了`pc`, 而没有进行任何处理. 因此在`vectored`模式下, `pc`寄存器的值就等于`stvec`, 而不是正确的中断向量地址.

由于使用的`qemu`为从`ubuntu`官方源下载的, 猜测错误在于该版本的`qemu`还未使用正确的处理逻辑.

`qemu-system-riscv64 --version`输出为:

```
QEMU emulator version 8.2.2 (Debian 1:8.2.2+ds-0ubuntu1.10)
Copyright (c) 2003-2023 Fabrice Bellard and the QEMU Project developers
```

## 6. 残留疑点

该部分处理逻辑在2019年已被更新. 而我们使用的`qemu`版本为8.2.0, 依理论应已修复。

在qemu仓库中发现该issue
https://gitlab.com/qemu-project/qemu/-/issues/2855

新的处理逻辑则是由commit [RISC-V: Add support for vectored interrupts](https://gitlab.com/qemu-project/qemu/-/commit/acbbb94e5730c9808830938e869d243014e2923a) 提交的, 该commit提交于2019-03-16.
但新处理逻辑直到10.1.0版本才被合并进master分支中, 因此先前版本中该问题仍然存在.

## 7. 解决方案

更新`qemu`至10.1.0.