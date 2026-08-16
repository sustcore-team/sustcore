# Sustcore Kernel

Sustcore 内核使用 freestanding C++23，当前支持 RISC-V64/SBI 与 LoongArch64/LABOOT。

活动 BSP 路径已经完成 BootInfo/FDT 持久化、PageDatabase、Buddy、全局 SLUB 堆、普通 C++
constructors、KernelSpace、KernelMM、boot/init 内存回收和 runtime trap vector 安装。

当前调度器仍只启用 BSP CPU，但内部已经采用 per-CPU `RunQueue`、内嵌于 Thread 的
`SchedulerStorage` 和无虚调用的 RR/FIFO `enter/leave/select` 调度类。`RunQueue::current` 是
current 的唯一来源，ready queue 只保存 `SchedEntity`；Thread 通过私有 owner token Adapter
在切换提交边界恢复。动态 worker 和用户 Thread 各自拥有 64 KiB 内核栈，所有上下文切换
经由双架构共同的 `__switch_to` ABI。

Thread 生命周期事件由 `SchedulerCore` 解释为
`attach/wake/block/yield/preempt/exit`。timer IRQ 先推进 precision timer engine，再评估 RR
抢占 deadline 并设置 `RunQueueFlags::NEED_RESCHED`；所有可恢复 trap 在统一出口调用
`SchedulerCore::schedule()`，由 Scheduler 重新选择并提交切换。precision timer 使用
宿主嵌入的 `PrecisionTimerNode` 和 typed virtual `Worklet`；IRQ 只把到期 completion
post 到固定 BSP WorkQueue，不在中断中执行虚函数。kinit 的 BSP Thread 在初始化完成后转为
同一 Thread 切换路径上的 idle。Scheduler、WorkQueue、timer 与 FIFO handoff 启动验证
由 `kernel/test/` 中的分阶段 selftest 统一执行。

KernelObject 基类与 `CSpace`、`MemorySegment`、`AddressSpace`、`Process`
和 `Thread` 位于 `kernel/obj/`，跨对象依赖可从 `obj/objfwd.h` 取得集中前向声明。

当前尚未实现调度器的 IPI、AP 启动、SMP placement/迁移、affinity 和 Fair/RT 策略；
precision timer、WorkQueue 和 timed wait 仍是 BSP 固定执行模型，跨 CPU wake 与
generation/ack 协议将在后续阶段接入。
历史目录与旧文档中关于完整 Stage 2/SMP 已就绪的描述不能用于判断当前运行行为。

内核开发约束见 `AGENTS.md`，早期初始化和并发边界见 `aidoc/kernel/early_setup.md`。
