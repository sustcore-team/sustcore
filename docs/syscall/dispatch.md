# Syscall Dispatch

本文总结 `kernel/syscall/syscall.*` 与 RISC-V trap 入口中 syscall 分发流程。该路径连接用户态寄存器参数、TCB syscall 生命周期、可挂起协程、调度器返回值提交和 capability 对象操作。

## 入口

用户态通过 RISC-V `ecall` 进入 trap。当前入口已经拆成两层:

1. `kernel/arch/riscv64/int/exception.cpp` 中的 `on_ecall_u()`
2. `kernel/syscall/syscall.cpp` 中的 `handle_user_ecall(...)`

其中:

- `on_ecall_u()` 只保留架构相关动作
- `handle_user_ecall(...)` 负责通用 syscall 生命周期

`on_ecall_u()` 负责:

1. 将当前 trap context 保存到 `env::inst().trap_context()`
2. 读取当前调度线程 `Scheduler::current_tcb()`
3. 调用 `Riscv64Context::read_args()` 读取参数
4. 构造 `task::SyscallContext`
5. `ctx->sepc += 4`，跳过 `ecall` 指令
6. 调用 `syscall::handle_user_ecall(...)`

参数寄存器约定:

- `a7`: syscall number。
- `a0`: capidx。
- `a1` 到 `a6`: 普通参数。

返回寄存器约定:

- `a0`: 返回值或 bool。
- `a1`: `ErrCode`。

## 返回包

syscall 返回值统一使用:

```cpp
struct RetPack {
    bool processed;
    b64 ret0;
    b64 ret1;
};
```

辅助函数会把 `Result<T>` 转成 `RetPack`:

- `result_void_ret()`
- `result_value_ret()`
- `result_bool_ret()`

失败时 `ret1` 保存错误码，成功时 `ret1 = ErrCode::SUCCESS`。

## active context

syscall 层维护当前 syscall 显式上下文:

```cpp
task::SyscallContext *active_context();
void set_active_context(task::SyscallContext *context);
```

`SyscallContext` 保存:

- `tcb`
- `pcb`
- `tmm`
- `trap_context`

同步分发和异步分发都会先设置 active context。对象操作如果需要当前线程，会优先读取该上下文，而不是直接依赖调度器全局 current。

## 同步分发

`dispatch_sync(tcb)` 处理不会挂起的 syscall。

主要流程:

1. 检查 `tcb->task` 有效。
2. 构造栈上 `SyscallContext`。
3. 设置 active context。
4. 拆出 syscall number、capidx 和六个参数。
5. switch 分发到具体 syscall helper。
6. 清空 active context。
7. 返回 `RetPack`。

同步分发返回后，trap handler 会调用:

```cpp
current_tcb->syscall_info.complete(ret);
```

实际写回用户寄存器由调度器稍后提交。

## 异步分发

当前可挂起 syscall 是:

- `SYS_NOTIF_WAIT`
- `SYS_ENDPOINT_SEND`
- `SYS_ENDPOINT_RECV`
- `SYS_ENDPOINT_CALL`

`dispatch_async(tcb)` 返回 `wait::cotask<void>`。它会在对象操作处
`co_await`，等待队列和调度器负责后续恢复。

异步 syscall 完成时会调用 `complete_syscall(tcb, ret)`:

1. 将 `SyscallInfo` 标记为 `COMPLETED`。
2. 如果线程在 `WAITING`，把状态改为 `EMPTY` 并唤醒。
3. 如果所属进程正在退出，则不重新入队。

## coroutine 挂起与线程等待

可挂起 syscall 当前通过 `wait::FutureAwaiter` 挂起 coroutine。

它与旧模型的关键区别是:

- `FutureAwaiter` 自己不直接操作 `TCB`
- `FutureAwaiter` 只写当前 coroutine 的 `WaitContext`
- `wait::cotask` 沿 `co_await` 链传播 `wait_wd`
- 最外层 syscall 路径根据 `wait_context().pending()` 再决定是否调用
  `register_syscall_wait(...)`

如果 `wait_context().pending()` 为真:

- TCB 加入等待队列
- TCB 状态变为 `WAITING`
- 设置 `NEED_RESCHED`

唤醒时，等待系统会清理队列元数据，并把线程重新转回可调度状态。当前文档不再假定调度器会在切线程前恢复最内层 coroutine handle。

## 调度器提交 syscall

调度器负责在合适时间提交 syscall 返回值:

- `Scheduler::schedule()` 会处理当前线程已完成 syscall。
- `Scheduler::can_schedule_tcb()` 会处理候选线程已完成 syscall。

提交动作:

```cpp
tcb->context()->write_ret(tcb->syscall_info.syscall_result);
tcb->syscall_info.reset();
```

如果线程仍处于 `EXECUTING`，调度器不会把它恢复到用户态。

## 用户缓冲区

syscall 中的用户指针通过 `UBuffer` / `UString` 访问。

`UBuffer` 行为:

1. 根据 active context 找到当前 TMM。
2. 定位用户地址所在 VMA。
3. 要求整个缓冲区落在同一个 VMA 中。
4. 找到对应 `MemoryPayload` 和偏移。
5. `sync_from_user()` 用 payload read 复制到内核缓冲。
6. `commit_to_user()` 用 payload write 写回用户空间。

这避免 syscall 层直接解引用用户虚拟地址。

## 当前限制

当前 syscall 分发仍有一些限制:

- active context 是单个全局指针，适配当前单核模型。
- 可挂起 syscall 目前只覆盖 endpoint 同步通信。
- `UBuffer` 不支持跨 VMA 缓冲区。
- 未知 syscall 返回 `ErrCode::NOT_SUPPORTED`。
