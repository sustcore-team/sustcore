# Sustcore Kernel

Sustcore 内核使用 freestanding C++23，当前支持 RISC-V64/SBI 与 LoongArch64/LABOOT。

活动 BSP 路径已经完成 BootInfo/FDT 持久化、PageDb、Buddy、全局 SLUB 堆、普通 C++
constructors、KernelVm、KernelMM、boot/init 内存回收和 runtime trap vector 安装。

调度器为每个 online CPU 提供固定地址的 `SchedCore`、`RunQueue`、current、idle 和
`DeadlineMux`；BSP WorkQueue 仍固定在 `CpuId{0}`。Thread 仍内嵌
`SchedStorage` 并使用无虚调用的 RR/FIFO `enter/leave/select` 调度类。`RunQueue::current` 是
current 的唯一来源，ready queue 只保存 `SchedEntity`；Thread 通过私有 owner token Adapter
在切换提交边界恢复。动态 worker 和用户 Thread 各自拥有 64 KiB 内核栈，所有上下文切换
经由双架构共同的 `__switch_to` ABI。

Thread 生命周期事件由 `SchedCore` 解释为
`attach/wake/block/yield/preempt/exit`。timer IRQ 先推进 precision timer engine，再评估 RR
抢占 deadline 并设置 `RunQueueFlags::NEED_RESCHED`；所有可恢复 trap 在统一出口调用
`SchedCore::schedule()`，由 Scheduler 重新选择并提交切换。precision timer 使用
宿主嵌入的 `HrTimer` 和 typed virtual `Worklet`；IRQ 只把到期 completion
post 到固定 BSP WorkQueue，不在中断中执行虚函数。kinit 的 BSP Thread 在初始化完成后转为
同一 Thread 切换路径上的 idle。Scheduler、WorkQueue、timer 与 FIFO handoff 启动验证
由 `kernel/test/` 中的分阶段 selftest 统一执行。

KObject 基类与 `CSpace`、`MemSeg`、`AddrSpace`、`Process`
和 `Thread` 位于 `kernel/obj/`，跨对象依赖可从 `obj/objfwd.h` 取得集中前向声明。

当前已实现 runtime IPI mailbox（`RESCHEDULE`、`TLB_SHOOTDOWN`、`STOP`）、页表 generation/ack
shootdown 和 AP 启动状态机。RISC-V SBI HSM 与 LoongArch LABOOT 共享 `ApBootArgs` ABI；pinned
`smp-init` Thread 在 commit 前允许失败 AP 降级，commit 后发布不可变 online set。trampoline 使用独立的
页对齐永久段，并仅保留一页 RX identity alias 用于最终 MMU 切换。

跨 CPU remote wake、固定 affinity、AP 本地 timer 抢占、同一 Process 的多 CPU 地址空间激活以及
多 CPU WorkQueue/allocator 压力 selftest 已注册到 POST_SMP_INIT；固件不支持次级 CPU 启动时
仍可降级为 BSP-only，但当前 QEMU 配置已实际验证远端 wake、AP timer 抢占及 shootdown acknowledgement。运行期迁移、
Fair/RT 策略和外部 IRQ affinity 仍不在第一版范围内。OpenSBI 启动日志中的
`Platform HSM Device : ---` 只表示平台没有单独的硬件 HSM 设备，不等于 SBI HSM 扩展不可用；Sustcore
通过 `sbi_probe_extension(SBI_EID_HSM)` 判断扩展，并在当前 QEMU RISC-V `-smp 4` 运行中实际完成
`HART_START`，日志可见 `started=4, online=4`。只有扩展探测或启动调用确实失败时才降级为 BSP-only。
历史目录与旧文档中关于完整 Stage 2/SMP 已就绪的描述不能用于判断当前运行行为。

内核开发约束见 `AGENTS.md`，早期初始化和并发边界见 `aidoc/kernel/early_setup.md`。
