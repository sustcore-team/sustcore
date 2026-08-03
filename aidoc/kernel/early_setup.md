# Sustcore 内核早期初始化

本文记录当前活动内核已经落地的初始化和并发边界。

## 当前 BSP 流程

```text
reset
  -> 早期 C++ 运行时
  -> PageDatabase / Buddy
  -> 全局 SLUB 堆
  -> 普通 C++ constructors
  -> KernelSpace / KernelMM
  -> 回收 boot 与 init 内存
  -> 安装 runtime trap vectors
  -> BSP 单核协作式 FIFO 调度
```

`kernel/boot` 只负责固件交接、BootInfo/FDT 校验与持久化、早期内存和最终页表切换；永久组合根
位于 `kernel/init/bsp.cpp`。普通全局构造函数只在堆进入 READY 后由 BSP 执行一次。

里程碑枚举中 firmware、IRQ、timer、SMP 等后续阶段目前仍是保留接口，活动启动路径尚未发布
这些阶段，不能据此声称相应子系统已经就绪。

## 当前调度边界

- 调度器只运行在 BSP，不启动 AP，也没有 per-CPU scheduler state；
- 调度策略为协作式 FIFO，只由 `yield()` 和 Thread 退出触发；
- run queue 使用 `tay::intrusive_list`，尾插和头取均为 O(1)，且不拥有 Thread；
- worker Thread 拥有页对齐的 64 KiB `KernelStack`，kinit 直接采用现有 BSP 栈；
- 所有切换通过两架构共同的 `__switch_to(Context *, Context *)` ABI；
- runtime interrupt、timer 抢占、idle、block/wake、跨 CPU wake 和迁移尚未实现；
- 本阶段按用户决议暂不建立 Process，Thread 的 Process 归属是下一阶段需要补齐的不变量。

调度器运行期间保持本地中断关闭。不得在尚未恢复 interrupt/timer dispatcher 前擅自打开中断
或把当前协作式路径描述为可抢占调度。

## 并发契约

- Sustcore 不使用 Big Kernel Lock；Buddy、SLUB、KernelSpace 等现有共享对象继续使用各自的
  irq-save 锁或原子协议；
- `PageTable` 自身不持锁，`KernelSpace` 与 `ClientSpace` 使用 `kernel::synchronized` 保护所属
  页表状态；
- 当前 run queue 仅由关闭中断的 BSP 执行流访问，因此不额外持有锁；恢复 SMP 前必须重新定义
  run queue 所有权与跨 CPU wake 协议；
- 当前 Thread 的栈只能在该 Thread 已经 `EXITED`、不在 run queue 且不再是 current 后回收。

## 当前验收

RISC-V64 与 LoongArch64 均已通过单 CPU QEMU bring-up：kinit、worker-1、worker-2 按 FIFO 顺序
各输出 10 次，worker 返回后经统一退出路径离开，随后由 kinit 回收其 TCB 和内核栈。
