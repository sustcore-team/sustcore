# Thread System

本文总结 Sustcore 的线程系统。线程由 `task::TCB` 表示，是调度器真正调度的实体；进程只是资源容器，包含线程链表、地址空间和 capability holder。

## TCB 结构

`task::TCB` 是线程控制块，字段可以分成五组。

### 基本信息

- `tid`: 线程 ID。
- `task`: 所属 PCB。
- `is_kernel`: 是否为内核线程。
- `kentry`: 内核线程入口。
- `karg`: 内核线程参数。
- `list_head`: 挂入所属 PCB `threads` 链表的节点。

### 内核栈与上下文

每个线程都有独立内核栈:

- `KSTACK_PAGES = 8`
- `KSTACK_SIZE = 32 KiB`
- `kstack_phy`: 内核栈顶对应物理地址记录。
- `kstack_bottom`: 内核栈顶虚拟地址。

用户线程的 trap 上下文保存在内核栈顶部附近，`TCB::context()` 会从 `kstack_bottom` 推导该上下文；内核线程则直接返回 `kernel_ctx`。

### 调度信息

- `schd_class`: 调度类。
- `basic_entity`: 通用调度元数据。
- `rr_entity`: RR 调度类额外实体。

`basic_entity` 中包含线程状态、runqueue 节点和 `FLAGS_NEED_RESCHED`。

### 等待信息

- `wait_head`: 挂入等待队列的节点。
- `wait_wd`: 当前等待描述符。
- `wait_predicate`: 唤醒前检查的谓词。

### syscall 信息

`TCB::SyscallInfo` 记录一次 syscall 的生命周期:

- syscall 参数。
- syscall 号。
- syscall 返回值。
- 状态: `NONE` / `EXECUTING` / `COMPLETED`。

当前 `SyscallInfo` 不再持有 coroutine handle 或显式 syscall 上下文；它只描述寄存器参数、返回值和执行状态。

## 线程初始化

### `alloc_tcb`

`TaskManager::alloc_tcb()` 从 KOP 分配 TCB，并初始化所有字段为空状态。

### `init_tcb`

`init_tcb(tcb, pcb)` 会:

1. 分配新 tid。
2. 设置所属 PCB。
3. 清空等待、syscall 和链表状态。
4. 通过 GFP 分配 4 页内核栈。
4. 通过 GFP 分配 8 页内核栈。
5. 计算 `kstack_phy` 和 `kstack_bottom`。

用户线程上下文由 `build_user_contexts()` 构造: 它会在内核栈中压入用户 trap 上下文，并把内核上下文入口设置为 `new_utask_trampoline`。内核线程上下文由 `build_kernel_context()` 直接构造到 `kernel_ctx`。

## 用户线程

### 主线程

`construct_main_thread()` 用于构造进程主线程:

1. 创建初始用户栈 `MemoryPayload`。
2. 把用户栈映射为 `STACK` VMA，范围是 `[USER_STACK_BOTTOM, USER_STACK_TOP)`。
3. 调用 `construct_thread()` 创建 TCB。
4. 创建 TCB capability。
5. 将 `StartupInfo` 和可选 startup blob 写入用户栈。
6. 调整初始 SP。

默认初始用户栈:

- `USER_STACK_TOP = 0x4000000000`
- 最大大小 `MAX_INITIAL_STACK_SIZE = 256 MiB`
- `USER_STACK_BOTTOM = USER_STACK_TOP - MAX_INITIAL_STACK_SIZE`

### 用户创建线程

`create_thread_current(entry, stack_addr, stack_size)` 为当前进程创建线程。

主要校验:

- 当前线程和所属 PCB 有效。
- 不能在内核进程中创建用户线程。
- 当前 PCB 必须有 tmm 和 holder。
- entry、stack_addr、stack_size 必须有效。
- 当前线程不能是 IDLE。
- `[stack_addr, stack_addr + stack_size)` 必须落在当前地址空间已有 VMA 中。

成功后:

1. 以当前线程调度类创建新 TCB。
2. 在当前 holder 中创建 `TCBPayload` capability。
3. 调用 `Scheduler::wakeup_new()` 唤醒新线程。
4. 返回 TCB capability 槽位。

## 线程回收

`recycle_tcb(tcb)` 会:

1. 拒绝回收当前正在运行的线程。
2. 若线程处于 READY，从调度器出队。
3. 若线程处于 WAITING，从等待队列移除。
4. 从所属 PCB `threads` 链表移除。
5. 释放内核栈物理页。
6. 清空 TCB 关键字段。
7. 删除 TCB 对象。

`terminate_tcb()` 当前只是转发到 `recycle_tcb()`。

## 等待系统

等待描述符由 `wait::WaitReasonManager` 管理。

核心结构:

- `wait::wd_t`: 等待描述符编号。
- `WaitQueue`: 单个等待描述符对应的 TCB 队列。
- `WaitPredicate`: 唤醒时针对 TCB 的谓词。
- `WaitReadyPredicate`: syscall awaiter 挂起前的就绪判断。

等待队列操作:

- `alloc_reason()`
- `enqueue(reason, tcb, predicate)`
- `peek_one(reason)`
- `pop_one(reason)`
- `remove(tcb)`
- `wake_one(reason)`
- `wake_all(reason)`
- `has_waiting(reason)`

普通等待线程被唤醒时会调用 `Scheduler::wakeup_waiting()`。当前等待子系统不依赖 TCB 中保存 coroutine handle；被唤醒线程只是从 `WAITING` 重新转回可调度状态。

## syscall 协程等待

`FutureAwaiter` 是当前可挂起 syscall 的通用等待点。

当前等待模型由三部分组成:

1. `FutureAwaiter`
2. `wait::WaitContext`
3. `wait::cotask`

`FutureAwaiter` 挂起时只负责:

1. 检查 ready predicate
2. 把 `wait_wd`、谓词和 suspended leaf 写入 `WaitContext`
3. suspend 当前 coroutine

它不直接:

- 修改 `TCB::SyscallInfo`
- 将线程加入等待队列
- 修改线程状态

这些动作由最外层 syscall 路径根据 `wait::cotask::wait_context()`
统一处理。若 `wait_context().pending()` 为真，则:

1. 将 TCB 加入等待队列
2. 将线程置为 `WAITING`
3. 设置 `FLAGS_NEED_RESCHED`

之后调度器会选择其它线程运行。

## 当前限制

当前线程系统仍有一些限制:

- TCB 没有用户态线程本地存储抽象。
- 用户线程创建要求调用方自己准备并映射栈区。
- `WaitReasonManager` 没有锁，当前更偏单核模型。
- 线程退出主要通过进程 kill 或 TCB 回收路径处理，用户态线程自退出接口尚不完整。
