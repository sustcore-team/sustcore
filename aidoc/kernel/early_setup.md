# Sustcore 内核早期初始化

本文记录当前活动内核已经落地的初始化和并发边界。

## 当前 BSP 流程

```text
reset
  -> 早期 C++ 运行时
  -> PageDb / Buddy
  -> 全局 SLUB 堆
  -> 普通 C++ constructors
  -> KernelVm / KernelMM
  -> 回收 boot 与 init 内存
  -> immutable device catalog / CPU topology
  -> 安装 runtime trap vectors
  -> IRQ / timer 就绪
  -> BSP scheduler / WorkQueue selftest
  -> BSP pinned smp-init Thread
  -> immutable SMP online snapshot
```

`kernel/boot` 只负责固件交接、BootInfo/FDT 校验与持久化、早期内存和最终页表切换；永久组合根位于
`kernel/init/bsp.cpp`。普通全局构造函数只在堆进入 READY 后由 BSP 执行一次。

`smp-init` 是固定在 BSP 的普通内核 Thread。它准备每 AP 的永久启动资源、发起固件启动、等待
`READY`，在 commit 前允许超时 AP 降级为 `ABANDONED`，随后一次性发布 immutable online snapshot 并
打开 release gate。已提交 CPU 未在预算内进入 `ONLINE` 会 panic，禁止静默缩减 online set。SMP
里程碑在该提交完成后发布。

## 当前调度边界

- `SchedCore[MAX_CPUS]`、`DeadlineMux[MAX_CPUS]` 使用固定地址 storage。AP 通过
  `ap_main()` 在本地绑定 CpuLocal、安装 runtime trap/IPI、激活 KernelVm、初始化本地 clock/
  deadline 与 scheduler 后发布 `READY`；
- `for_cpu()` 只接受已经 online 的 CPU，bring-up 仅能在目标 AP 上经 `prepare_cpu()` 取得未发布的
  SchedCore；BSP WorkQueue 仍固定在 CPU 0；
- `SchedCore::debug_state()` 可在队列锁内复制当前队列快照，这些字段仅用于诊断；
- 调度策略包含 FIFO 与 RR；`yield()`、Thread 退出、block/wake 以及本地 timer IRQ 的 RR deadline
  都会在统一 trap-return 安全点请求重新选择；
- run queue 使用 `tay::intrusive_list`，尾插和头取均为 O(1)，且不拥有 Thread；
- worker Thread 拥有页对齐的 64 KiB `KernelStack`，kinit 直接采用现有 BSP 栈；
- 所有切换通过两架构共同的 `__switch_to(Context *, Context *)` ABI；
- runtime IPI mailbox 已支持 `RESCHEDULE`、`TLB_SHOOTDOWN` 与 `STOP`。AP 具备本地 timer
  source/deadline state；跨 CPU wake、固定 placement 和 AP 本地 timer 抢占已接入 SMP selftest，
  但只有 AP online 的固件运行才能提供真实远端证据。
- 本阶段暂不建立 Process，Thread 的 Process 归属是后续阶段需要补齐的不变量。

调度器只在提交 `current`、选择下一个 Thread 和执行 `__switch_to()` 的局部临界区关闭中断；普通
Thread 运行期间本地 timer IRQ 可以请求 RR 抢占，并由统一 trap-return 路径执行切换。跨 CPU remote
wake 严格先在目标 run queue 入队，再在释放队列锁后发送 `RESCHEDULE` IPI；该顺序已由 RISC-V 与
LoongArch QEMU `-smp 4` 的远端唤醒 selftest 实际验证。

## 并发契约

- Sustcore 不使用 Big Kernel Lock；Buddy、SLUB、KernelVm 等共享对象继续使用各自的 irq-save 锁或原子协议；
- `PageTable` 自身不持锁，`KernelVm` 与 `UserVm` 使用 `kernel::synchronized` 保护所属页表状态；
- 每个 run queue 由所属 CPU 在关闭本地中断时访问；跨 CPU wake 严格先进入目标队列，再发送
  `RESCHEDULE` IPI，第一版不提供运行期迁移；
- 当前 Thread 的栈只能在该 Thread 已经 `EXITED`、不在 run queue 且不再是 current 后回收。

阶段 1 的锁调用点分类如下：

| 调用点 | 保护语义 | 约束 |
| --- | --- | --- |
| `kernel::synchronized<>`、`kernel::lock_guard` | 仅禁止抢占 | 状态不由本地硬中断直接访问 |
| logger、RISC-V PLIC | `irq_synchronized<>` / `irq_locked_ref` | 普通 Thread 与本地 IRQ 共享，锁内不阻塞 |
| scheduler、DeadlineMux、PrecisionTimer、WorkQueue | `hal::irq_guard` + raw `tay::lock_guard` | 保留现有 IRQ-off 临界区，迁移时保持锁序 |
| IRQ registry、C++ runtime | `kernel::lock_guard` | writer 只需抢占保护，handler 读路径不阻塞 |
| AddrSpace、MemSeg、Cspace、Buddy、SLUB、Catalog | `kernel::lock_guard` 或 `synchronized<>` | 跨 CPU 前须重新审计所有权 |

新代码不得引入未分类的 raw `tay::lock_guard`；保留调用点都位于显式 IRQ-off 临界区或对应的初始化/运行时封装内。

阶段 1 已建立固定地址的 BSP `CpuLocal` 和 `hal::preempt_guard`。`kernel::synchronized<>` 现在只
禁止抢占；可能由本地硬中断重入的状态必须显式使用 `kernel::irq_synchronized<>`。两架构 runtime trap
采用 `scratch -> CpuLocal -> kstack_top` 协议：内核 C++ 执行期间 `tp` 指向 CpuLocal，
用户 `tp` 只保存在 TrapFrame 并在返回前恢复。抢占禁止期间的调度请求会记录在 CpuLocal，最外层 guard
或 IRQ 恢复路径在安全上下文触发统一 checkpoint。次级 CPU trampoline 位于单独永久保留、页对齐的
`.smp.trampoline`，最终 KernelVm 仅提供这一页 RX identity alias 以跨越 MMU 切换；其余低地址
映射不会因 AP bring-up 扩张。AP 生命周期、本地 timer、跨 CPU wake 和目标队列入队协议已实现；
当前 BSP-only 运行只能证明本地路径，不能替代远端 CPU 验证。

页表修改已接入全局串行化的 TLB shootdown coordinator：PTE release 发布后，发起 CPU 在持有所属
PageTable 锁的情况下发布 generation/request，先执行本地 flush，再以 release 方式发布 generation 并
向其余 online CPU 发送 `TLB_SHOOTDOWN` IPI。远端 handler 只 acquire 读取 generation、执行本地 flush
并 release 写 acknowledgement，不获取 PageTable 或 coordinator 锁。所有 target ack 后才允许
`RetirementSink::retire_all()` 回收脱链页表页。BSP-only fallback 仍覆盖 generation、本地
acknowledgement 与 TLB self-IPI handler；在 RISC-V 与 LoongArch QEMU 的 `-smp 4` 运行中都已实际
观察远端 ack。

## 当前共享状态契约

- Catalog 在 BSP 枚举完成后以 release 发布，之后只读；CPU topology 同样以不可变 snapshot 提供。
- Process/ProcTable、AddrSpace/VMA 与 MemSeg 各自使用 preempt-safe 对象锁；Process 锁
  在进入 AddrSpace 或 scheduler 前释放，AddrSpace activation 只写 CpuLocal。
- IRQ registry 使用 preempt-safe writer lock 与 RCU reader；logger 使用 IRQ-safe guard，panic 通过原子
  owner、STOP IPI 与 emergency console 避免等待普通 logger lock。
- C++ guard 以执行 token 区分递归和竞争；Buddy、PageDb ownership 与 SLUB 只允许普通任务上下文
  访问，硬中断路径会在 debug 下触发断言。

固定锁序包括：PageTable -> TLB coordinator（IPI handler 不取二者）；KernelMM layout lock 在调用页表
事务前释放；Process lock 在 scheduler 操作前释放；Buddy 覆盖 PageDb claim/release，SLUB 不持其
state lock 调 Buddy；logger 是叶子锁。

## 当前验收

RISC-V64 与 LoongArch64 均已通过 QEMU bring-up、runtime IPI、TLB shootdown、remote wake、
多 CPU work Threads 和 allocator/object selftest。OpenSBI 的 `Platform HSM Device : ---` 是“无专用
HSM 设备”的平台诊断，不代表 SBI HSM 扩展不可用；RISC-V QEMU `-smp 4` 已通过扩展探测和
`HART_START`，实测 `started=4, online=4`。LoongArch LABOOT `-smp 4` 也已通过 MBUF0/MBUF1
trampoline handoff，实测 `started=4, online=4`。固件确实不支持次级 CPU 启动时仍保留 BSP-only
降级路径，但不能把这种降级运行当作 AP 正确性证明。
