# 锁与受保护对象

taycpplib 将原子同步、锁所有权和执行上下文保护拆成独立类型。基础锁不依赖
kernel 的中断、抢占或调度器接口；kernel 可以把自己的 RAII Context Guard
组合到同一套锁接口中。

锁类型位于 `<tay/spinlock.h>`，其余通用设施位于 `<tay/lock.h>`。
`<tay/raii.h>` 也会包含后者。

## 自旋锁

`tay::spinlock` 是较小的非公平锁。它使用 test-and-test-and-set：竞争失败后先
以 relaxed 顺序读取状态，只有观察到锁被释放才再次执行原子交换。这可以减少
多个 CPU 持续写同一 cache line 的争用。

`tay::ticket_spinlock` 为每次获取分配递增票号，并等待
`serving_ticket == ticket`，因此按票号提供 FIFO 公平性。票号为无符号 32 位
整数；相等性判断在计数回绕后仍成立。该类型要求目标提供 lock-free 32 位
原子操作。

两种锁都提供：

```cpp
void lock() noexcept;
[[nodiscard]] bool try_lock() noexcept;
void unlock() noexcept;
```

成功获取使用 acquire，释放使用 release。x86 等待循环发出 pause；当前
RISC-V、LoongArch 和未知目标使用 compiler barrier，不会假定 RISC-V 已启用
Zihintpause。

两种锁均不可复制、不可移动，也不公开 `is_locked()`。瞬时锁状态不能用于
同步；需要表达“当前代码拥有锁”时应查询 `unique_lock::owns_lock()`。

基础自旋锁不会关闭中断或禁止抢占。如果某把锁也会在本 CPU 的中断处理程序
中获取，任务上下文必须在获取锁前通过 kernel 提供的 Context Guard 保存并
关闭本地中断。

## `lock_guard`

`tay::lock_guard<Lock>` 适用于固定词法作用域。普通构造会立即调用
`lock()`，析构调用 `unlock()`；`adopt_lock` 构造表示调用者已经持有锁。
该 guard 不可复制、不可移动，也不提供提前解锁接口。

```cpp
tay::spinlock lock;

void update() {
    tay::lock_guard guard{lock};
    // 临界区
}
```

`adopt_lock` 是前置条件接口：传入的锁必须已由当前执行上下文获取。

## `unique_lock`

`tay::unique_lock<Lock>` 可以为空、延迟获取、接管已持有锁，或尝试获取锁：

```cpp
tay::unique_lock<tay::spinlock> empty;
tay::unique_lock immediate{lock};
tay::unique_lock deferred{lock, tay::defer_lock};
tay::unique_lock adopted{lock, tay::adopt_lock};
tay::unique_lock attempted{lock, tay::try_to_lock};
```

它可移动但不可复制，提供 `lock()`、`try_lock()`、`unlock()`、
`owns_lock()`、`mutex()`、`release()` 和 `swap()`。移动赋值会先释放目标当前
拥有的锁。`release()` 只返回锁指针并放弃 RAII 责任，不会解锁。

下列调用属于契约破坏并调用 `tay::panic()`：

- 对没有关联锁的对象调用 `lock()` 或 `try_lock()`；
- 已经拥有锁时再次调用 `lock()` 或 `try_lock()`；
- 未拥有锁时调用 `unlock()`。

普通锁竞争由阻塞或 `try_lock()` 的 false 结果表达，不使用异常。

## 执行上下文 Guard

`guard_stage<Order, Guard>` 为默认可构造的 RAII Guard 指定编译期顺序，
`context_lock_guard<Lock, Stages...>` 在获取锁前进入这些 Guard：

```cpp
using protected_lock = tay::context_lock_guard<
    tay::spinlock,
    tay::guard_stage<200, preempt_guard>,
    tay::guard_stage<100, irq_save_guard>>;
```

模板参数的书写顺序不影响结果。stage 会在类型实例化时按 Order 升序排序，
并组成真实的递归成员链；没有运行时排序、间接调用或“已构造但尚未进入”的
中间状态。上例的生命周期顺序为：

```text
irq_save_guard 构造
  -> preempt_guard 构造
    -> spinlock.lock()
    -> spinlock.unlock()
  -> preempt_guard 析构
-> irq_save_guard 析构
```

所有模板参数必须是 `guard_stage`，且 Order 不得重复。建议由 kernel 统一定义
带间隔的顺序常量和经过审核的组合别名，而不是由普通调用点散布顺序编号。
taycpplib 本身不提供 IRQ 或 preempt guard。

## `synchronized`

`tay::synchronized<T, Lock, Locker>` 同时拥有值和保护它的锁。默认锁为
`tay::spinlock`，默认 Locker 为 `tay::lock_guard`：

```cpp
struct device_state {
    bool ready = false;
    int count = 0;
};

tay::synchronized<device_state> state;

{
    auto access = state.lock();
    access->ready = true;
    ++access->count;
}
```

`lock()` 返回不可复制、不可移动的 `locked_ref`，提供 `get()`、`operator*`
和 `operator->`。访问句柄析构时由 Locker 释放锁。const `synchronized` 返回
`locked_ref<const T, ...>`，因此只允许读取数据。

`synchronized` 将构造参数转发给 `T`，自身不可复制、不可移动。默认类型不
保存外部对象指针；需要保护外部对象时应由上层显式管理其生命周期，而不是把
非拥有语义混入此类型。
