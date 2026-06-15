# Scheduler Base

本文总结 `kernel/schd/schdbase.h` 与 `kernel/task/scheduler.*` 中的线程调度框架。调度器以 `TCB` 为调度单元，按调度类优先级选择下一个线程，并在切换时同步当前 PCB、TCB 和地址空间。

## 调度单元

调度器并不直接调度 PCB，而是调度 `task::TCB`。

每个 TCB 中有:

- `schd_class`: 调度类。
- `basic_entity`: 通用调度元数据。
- `rr_entity`: RR 调度类专用数据。

`basic_entity` 类型是 `schd::SchedMeta`，包含:

- `state`
- `rq_head`
- `flags`

当前 flags 只有:

- `FLAGS_NEED_RESCHED`: 需要重新调度。

## 线程状态

`ThreadState` 定义在 `schdbase.h`:

- `EMPTY`: 不在运行队列中。
- `INITIALIZATION`: 初始化中。
- `READY`: 可运行，等待被选中。
- `RUNNING`: 正在运行。
- `YIELD`: 已主动让出，当前实现使用较少。
- `WAITING`: 等待事件，不参与普通调度。

调度类在 enqueue、dequeue、pick、put_prev 中负责维护这些状态。

## 调度类优先级

`ClassType` 的数值越大优先级越高:

1. `RT`
2. `INIT`
3. `RR`
4. `FCFS`
5. `IDLE`
6. `BOT`

`BOT` 只是遍历下界，没有实际调度类。

`Scheduler::foreach_schdclass()` 按优先级从高到低遍历:

```cpp
RT -> INIT -> RR -> FCFS -> IDLE
```

`pick_next_task()` 会按这个顺序查询每个调度类的 `pick_next()`，找到第一个可运行线程。

## 运行队列

`RQ` 是当前 hart 的运行队列集合，保存在 `env::hart_ctx` 中。

当前包含:

- `rt_list`
- `fcfs_list`
- `rr_list`

`INIT` 和 `IDLE` 是单槽调度类，不使用 `RQ` 链表保存 ready 队列。

## 调度类接口

所有调度类继承:

```cpp
template <typename SU>
class BaseSched
```

必须实现:

- `enqueue(rq, unit)`
- `dequeue(rq, unit)`
- `pick_next(rq)`
- `put_prev(rq, unit)`
- `yield(rq)`
- `on_tick(rq, unit)`
- `check_preempt_curr(rq, new_su)`

`BaseSched` 通过 `SchedMeta::as_entity()` 和 `SchedMeta::asunit()` 在 `TCB` 与内嵌调度实体之间转换。

## Scheduler 单例

`schd::Scheduler` 是显式初始化单例:

- `Scheduler::init(idle_tcb, init_tcb)`
- `Scheduler::initialized()`
- `Scheduler::inst()`

构造时会:

1. 将 idle TCB 标为 `RUNNING`。
2. 设置 IDLE 调度类当前线程。
3. 将 init TCB 标为 `READY`。
4. 设置 INIT 调度类 ready 槽。
5. 给 idle TCB 设置 `NEED_RESCHED`，让第一次调度切到 init。

## 地址空间切换

`Scheduler::switch_to(tcb)` 会:

1. 调用 `switch_pgd(tcb->task->tmm)`。
2. 更新 `env::hart_ctx->current_tcb()`。
3. 更新 `env::hart_ctx->current_pcb()`。

`switch_pgd()` 会在目标页表非空且不同于当前页表时:

- 调用 `PageMan(tmm->pgd()).switch_root()` 写 `satp`。
- 调用 `flush_tlb()`。
- 更新 `env::inst().tmm()`。

因此调度切换同时完成运行线程和地址空间切换。

## 调度流程

`schedule()` 的主流程:

1. 获取 current TCB。
2. 若线程未设置 `NEED_RESCHED`，直接返回。
3. 清除当前线程的 `NEED_RESCHED`。
4. 调用 `prepare_prev_task()` 将当前线程放回对应调度队列或标记为等待。
5. 调用 `prepare_next_task()` 选择下一个线程，并在选择时切换到候选线程的页表。
6. 必要时执行 RISC-V 内核上下文切换。

`prepare_next_task()` 会反复:

1. 调用 `pick_next_task()`。
2. 调用 `prepare_switch(next)`，提前切换到候选线程的页表并更新当前 PCB/TCB。
3. 如果候选线程可以直接运行则返回。
4. 否则给候选线程设置 `NEED_RESCHED` 并继续重试。

## 抢占检查

`check_preempt_curr(new_tcb)` 的规则:

1. 若没有当前线程，不抢占。
2. 若新线程调度类优先级不高于当前线程，不抢占。
3. 否则调用新线程所属调度类的 `check_preempt_curr()`。
4. 若返回 true，则给当前线程设置 `NEED_RESCHED`。

当前各调度类的 `check_preempt_curr()` 都采用“高优先级新线程抢占当前线程”的简单策略。

## tick 与 yield

`do_tick(event)` 会把 tick 事件交给当前线程所属调度类的 `on_tick()`。RR 调度类会在这里消耗时间片；其它当前调度类基本为空操作。

`yield()` 会调用当前调度类的 `yield()`，然后调用 `schedule()`。

## 阻塞与唤醒

`block_current(reason, predicate)` 会:

1. 获取当前线程。
2. 拒绝 IDLE 线程阻塞。
3. 将线程加入等待队列。
4. 设置 `NEED_RESCHED`。
5. 调用 `schedule()`。

`wakeup_waiting(tcb)` 会把 WAITING 线程状态置为 `EMPTY`，再调用 `wakeup()` 入队。

`wakeup_new(new_tcb)` 用于新线程和新进程主线程唤醒，当前直接转发到 `try_wakeup()`。

## 架构上下文切换

调度器依赖 RISC-V 汇编入口:

- `riscv64_kernel_switch(prev_ctx, next_ctx)`
- `riscv64_kernel_switch_to_user(prev_ctx, next_kstack)`
- `isr_restore_user(kstack_bottom)`
- `isr_restore_kernel(kstack_bottom)`

内核线程切换会保存 callee-saved 寄存器和恢复目标上下文。用户线程通常通过 trap 返回路径恢复完整上下文。

## 当前限制

当前调度系统仍有一些限制:

- 当前更偏单 hart 模型，`RQ` 来自当前 `env::hart_ctx`。
- 调度队列操作没有显式锁。
- `RT` 当前是 FIFO 风格，不含优先级或 deadline。
- `INIT` 和 `IDLE` 是单槽调度类。
- `YIELD` 状态尚未形成独立语义。
