# Task Syscall Lifecycle

本文总结任务系统内部如何承载 syscall 生命周期。更具体的任务 syscall API 见 `docs/syscall/task.md`；本文侧重 TCB、trap、scheduler 与可挂起 syscall 的协作。

## 参数保存

RISC-V 用户态通过 `ecall` 进入内核。当前路径已经拆成:

- 架构层 `on_ecall_u()`
- 通用层 `syscall::handle_user_ecall(...)`

其中架构层负责:

1. 从 `Riscv64Context` 读取 syscall 参数。
2. 构造 `task::SyscallContext`。
4. 将 `ctx->sepc += 4`，跳过 ecall 指令。
5. 调用通用 syscall 生命周期入口。

通用层负责:

1. `current_tcb->syscall_info.begin(args)`
2. 根据 syscall 号执行同步分发

`Riscv64Context::read_args()` 的寄存器约定是:

- `a7`: syscall number。
- `a0`: capidx。
- `a1` 到 `a6`: 六个普通参数。

返回值通过 `Riscv64Context::write_ret()` 写回:

- `a0`: ret0。
- `a1`: ret1，通常是 `ErrCode`。

## `SyscallInfo`

每个 TCB 内嵌一个 `SyscallInfo`:

- `syscall_args`
- `syscall_number`
- `syscall_result`
- `syscall_state`

状态机:

- `NONE`: 无 syscall。
- `EXECUTING`: syscall 正在执行。
- `COMPLETED`: syscall 已产生返回值，等待 trap 返回路径写回用户上下文。

`begin()` 会清空旧状态并进入 `EXECUTING`。`complete(ret)` 缓存返回值并进入 `COMPLETED`。

## 显式 syscall 上下文

`task::SyscallContext` 包含:

- `tcb`
- `pcb`
- `tmm`
- `trap_context`

轻量 syscall coroutine 不能依赖动态全局 current task，因此 syscall 层通过 `syscall::set_active_context()` 设置当前上下文。对象方法若需要当前线程，也会优先使用 `syscall::active_context()`。

这套显式上下文主要服务于对象方法和用户缓冲区访问辅助；当前 `TCB::SyscallInfo` 本身不保存 `SyscallContext`。

## 同步 syscall

非可挂起 syscall 走 `dispatch_sync(tcb)`。

流程:

1. 构造栈上 `SyscallContext`。
2. 设置 active context。
3. 根据 syscall number 执行具体 syscall。
4. 返回 `RetPack`。
5. trap handler 调用 `syscall_info.complete(ret)`。

同步 syscall 在本次 trap 中立即完成。真正写回用户寄存器发生在调度器的提交阶段。

## 可挂起 syscall

当前仓库中的 syscall 主路径以同步分发为主，`kernel/syscall/syscall.cpp` 中直接调用 `dispatch_sync(...)` 并以 `syscall_info.complete(ret)` 标记完成。等待子系统中的 `FutureAwaiter` / `wait::cotask` 已存在，但 `TCB::SyscallInfo`、调度器和 trap 主路径并没有保存或恢复 coroutine handle。

## 用户缓冲区访问

syscall 参数中的用户指针不会直接解引用，而是通过:

- `UBuffer`
- `UString`

`UBuffer` 会:

1. 解析当前 syscall 所属 TMM。
2. 定位用户地址所在 VMA。
3. 要求整个缓冲区落在同一个 VMA 中。
4. 找到对应 `MemoryPayload` 和偏移。
5. 用 `MemoryPayload::read/write` 在用户内存和内核缓冲之间同步。

`UString` 是只读字符串代理，会从用户空间同步最多指定长度，并用 `strnlen()` 计算长度。

## trap 返回前调度

`handle_trap()` 在处理完用户态 trap 后会:

1. 执行 syscall 或其它异常/中断处理。
2. 调用 `Scheduler::schedule()`。
3. 根据是否从 U-Mode 返回，更新 `sscratch = tcb->kstack_bottom`。
4. trap 汇编出口依据保存的上下文恢复用户线程或内核线程。

因此 syscall 返回值写回、线程切换和用户上下文恢复都在 trap 返回路径中完成闭环。

## 当前限制

当前 syscall 生命周期仍有一些限制:

- `UBuffer` 要求缓冲区不能跨 VMA。
- syscall active context 是单个全局指针，更适合当前单核模型。
- 若未来重新启用 coroutine 异步 syscall，文档中的调度恢复语义需要与实现再次对齐。
