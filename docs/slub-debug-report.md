# SLUB 分配器迁移调试报告

## 概述

将 sustcore 内核的全局分配器从 `LinearGrowAllocator`（线性增长分配器，简称 LGA）切换到 `slub::MixedSizeAllocator`（SLUB 分配器）后，出现多种崩溃。根本原因共 5 个独立的 bug，全部被 LGA 的 no-op `free()` 掩盖。

---

## 两个分配器的关键差异

| 特性 | LinearGrowAllocator | slub::MixedSizeAllocator |
|------|---------------------|--------------------------|
| `malloc()` | 从静态堆或 GFP 分配 | 按 2 的幂大小分派到 FixedSizeAllocator slab |
| `free()` | **空操作**（仅打 DEBUG 日志） | 真正释放 + 查 `alloc_records` 链表 + 归还到 slab |
| `init()` | 纯日志 | 重建所有 SLUB 对象（含 `alloc_records`、各 FixedSizeAllocator） |
| 内存复用 | 永不复用 | 释放后立即可被重新分配 |

**核心原理**：LGA 的 `free()` 什么都不做 → 内存从不复用 → 所有 use-after-free / double-free / 链表损坏都被掩盖。SLUB 真正释放并复用内存 → 这些 bug 立即暴露。

---

## Bug #1：`EndpointPayload` 重复释放 `EndpointMessage`

### 位置
`kernel/object/endpoint.cpp` — `EndpointPayload::~EndpointPayload()`

### 原因
`EndpointObject::send()` 在无等待接收者时，`EndpointMessage` 被同时放入两个地方：

```cpp
// send() 第 231 行
_obj->messages.push_back(*pending_ptr->message);    // 加入 messages 列表
// pending_ptr->message 未置空！仍持有同一指针
```

析构时两个循环先后 `delete` 同一地址：

```cpp
// 旧代码
~EndpointPayload() {
    while (!messages.empty()) {
        delete &messages.front();        // 第一次 free
    }
    while (!pending_sends.empty()) {
        delete pending->message;         // 第二次 free ← 同一地址！
    }
}
```

### 后果
SLUB：第一次 free 删除 `alloc_records` 中的记录，第二次 free 找不到记录 → `slub.h:550` 报错。内存被放回 freelist 两次 → freelist 出现环 → 后续分配返回已用内存 → 连锁损坏。

LGA：两次 `free()` 都是空操作，无声无息。

### 修复

先处理 `pending_sends`（将其关联 message 从 `messages` 摘除再 delete），再清理剩余的 `messages`：

```cpp
while (!pending_sends.empty()) {
    auto *pending = &pending_sends.front();
    pending_sends.pop_front();
    if (pending->message != nullptr) {
        if (message_is_queued(this, pending->message))
            messages.remove(*pending->message);  // 先摘除
        delete pending->message;
        pending->message = nullptr;
    }
    delete pending;
}
// 再清理未被 pending_send 引用的剩余消息
while (!messages.empty()) { delete &messages.front(); messages.pop_front(); }
```

---

## Bug #2：Cancel callback 在对象析构后仍可被触发

### 位置
`kernel/object/endpoint.cpp` — `~EndpointPayload()`、`~ReplyPayload()`

### 原因
`PendingEndpointSend` / `PendingEndpointRecv` / `PendingReplyRecv` 的 cancel callback 通过 `std::function` 存储在 `Promise` 的共享状态中。callback 捕获了 `payload` 和 `pending_ptr` 裸指针。

```cpp
pending_ptr->promise.set_cancel_callback([payload = _obj, pending_ptr]() {
    delete pending_ptr->message;
    delete pending_ptr;        // 释放 pending 对象
});
```

析构函数 `delete pending` 后，如果 `Future` 仍持有共享状态（refcount > 0），后续 `Future::cancle()` 会触发 callback，对已释放的 `payload` / `pending_ptr` 执行 `delete` → **double-free**。

### 后果
同 Bug #1：SLUB freelist 被污染，连锁损坏 `alloc_records`。

LGA：`delete` 什么也不做，callback 在已释放内存上操作也无所谓。

### 修复
在 `delete pending` 前清空 callback：

```cpp
pending->promise.set_cancel_callback({});  // 清空 callback
delete pending;
```

`~ReplyPayload()` 同样处理。

---

## Bug #3：`MixedSizeAllocator::init()` 缺少显式析构调用

### 位置
`kernel/mem/slub.h` — `MixedSizeAllocator::init()`、`FixedSizeAllocator::init()`

### 原因
内核从物理地址（pre-init）切换到虚拟地址（post-init）后，需要重建分配器。原始代码使用 C++ placement new 直接覆盖旧对象：

```cpp
// 旧代码
static void init() {
    new (&alloc_records) util::IntrusiveList<AllocRecord>();        // 直接覆盖！
    new (&ALLOC_RECORD_SLUB) SlubAllocator<AllocRecord>();          // 直接覆盖！
    Helper::init();
    // FixedSizeAllocator::init() 同样问题：
    //   new (&SLUB) slub::SlubAllocator<Object>();
}
```

**C++ placement new 不调用旧对象的析构函数**，直接覆写内存。后果：

1. 旧 `alloc_records` 的 `D_size` 未清零，分配记录丢失
2. 旧 SLUB 的 `partial`/`full`/`empty` 链表未 clear，旧 slab 的 `list_head` 仍指向被覆盖的旧 sentinel 地址
3. 后续 `to_empty`/`to_partial` 操作旧 slab 时，`erase()` 基于垃圾指针修改新链表 → `D_size` 下溢 → sentinel 损坏

### 修复
先显式调用析构函数（正确 unlink 所有节点，清零 `D_size`），再 placement new：

```cpp
static void init() {
    alloc_records.~IntrusiveList();           // 先析构
    new (&alloc_records) util::IntrusiveList<AllocRecord>();
    ALLOC_RECORD_SLUB.~SlubAllocator();       // 先析构
    new (&ALLOC_RECORD_SLUB) SlubAllocator<AllocRecord>();
    Helper::init();
}

// FixedSizeAllocator::init() 同样修复：
static void init() {
    SLUB.~SlubAllocator();                    // 先析构
    new (&SLUB) slub::SlubAllocator<Object>();
}
```

---

## Bug #4：`TaskMemoryManager` 析构时遍历已释放节点

### 位置
`kernel/mem/vma.cpp` — `TaskMemoryManager::~TaskMemoryManager()`

### 原因

```cpp
// 旧代码
~TaskMemoryManager() {
    auto &&list = std::move(vma_list);
    for (VMA &vma : list) {
        unmap_pages(vma.varea);
        delete util::owner(&vma);   // 释放 VMA
    }
    // ++it 读 vma.list_head.next → 已释放内存 → 垃圾值！
}
```

`for-range` 循环在 `delete` VMA 后，`++it` 会读取已释放 VMA 的 `list_head.next` 来推进迭代器。LGA 下内存不回收，`next` 值完好；SLUB 下内存复用后 `next` 被覆盖为垃圾值（如 `0x51`），导致 `IntrusiveList<VMA>::U_link` 收到非法指针崩溃。

### 修复

改用 `pop_front()` 循环：`pop_front()` 在 delete 前读取 `next`（节点仍在链表中时）并摘除节点：

```cpp
~TaskMemoryManager() {
    while (!vma_list.empty()) {
        VMA &vma = vma_list.front();
        vma_list.pop_front();      // 先读取 next，再摘除
        unmap_pages(vma.varea);
        delete util::owner(&vma);  // 此时节点已脱离链表，释放安全
    }
}
```

---

## Bug #5：中断重入导致 `alloc_records` 链表损坏（**核心 bug**）

### 位置
`kernel/mem/slub.h` — `MixedSizeAllocator::free()` / `malloc()`

### 原因

`free()` 执行期间未禁用中断：

```
free(ptr)
  └─ get_record(ptr)            // 找到 record R
  └─ _free(ptr, rsz)
       └─ inner_free(ptr)
            └─ slab_header->freelist = ptr   // 修改 SLUB 元数据
            └─ inuse == 0 → to_empty()
                 └─ empty.erase(slab)        // 修改 IntrusiveList
                      └─ U_link(prev, next)  // *** 中断可能在此发生 ***
                           └─ ISR 中调 operator delete
                                └─ MixedSizeAllocator::free()  ← 重入！
                                     └─ remove_record(...)    // 修改 alloc_records
  └─ remove_record(R)           // 外层回来：找不到 R 了！
```

### 调试证据

添加重入检测后，日志明确显示：

```
[MEMORY:ERROR]: MixedSizeAllocator::free REENTRANCY detected! ptr=0xffffffc08006d180
[MEMORY:ERROR]: MixedSizeAllocator::free REENTRANCY detected! ptr=0xffffffc08006d1b0
[MEMORY:ERROR]: MixedSizeAllocator::free REENTRANCY detected! ptr=0xffffffc08006d1b0  ← 同一地址三次
[MEMORY:ERROR]: remove_record: record=0xffffffc0800c6f10 NOT found in list (size=876)
```

同一地址 `0xffffffc08006d1b0` 被嵌套 free 了多次，证明中断处理程序反复释放同一个对象。

### 后果

1. **`remove_record NOT found`**：外层 `free` 找到的记录，被内层（中断）`free` 的 `remove_record` 从链表删除了，外层再删找不到。
2. **`alloc_records size mismatch`**：`D_size` 和实际可遍历节点数不一致（差值恰好 1），说明有一个节点被 erase 了但 `D_size` 未同步。
3. **连锁崩溃**：`alloc_records` 损坏 → `get_record` 遍历到垃圾指针（如 `0xd`、`0xe`）→ page fault。

### 为什么 LGA 不报错

`LinearGrowAllocator::free()` 是空操作 → 中断里调 `free` 什么也不发生 → 链表永不修改 → 重入安全。

### 修复

在 `malloc()` 和 `free()` 开头加入中断保护，禁用 S-Mode 中断直到函数返回：

```cpp
// 用内联 RISC-V CSR 指令实现 RAII 中断守卫
// 不用标准 InterruptGuard 是为了避免循环 include（driver/int/base.h → alloc.h → slub.h）
struct IrqGuard {
    bool was_enabled;
    IrqGuard() {
        unsigned long s;
        asm volatile("csrr %0, sstatus" : "=r"(s));
        was_enabled = (s & 2) != 0;
        asm volatile("csrci sstatus, 2" ::: "memory");  // 清除 SIE 位
    }
    ~IrqGuard() {
        if (was_enabled)
            asm volatile("csrsi sstatus, 2" ::: "memory");  // 恢复 SIE 位
    }
};

static void *malloc(size_t sz) {
    IrqGuard irq_guard;   // 禁用中断
    // ...
}

static void free(void *ptr) {
    IrqGuard irq_guard;   // 禁用中断
    // ...
}
```

---

## 调试工具

### 添加的诊断日志

| 日志位置 | 内容 | 用途 |
|---------|------|------|
| `init()` 开始 | `records=X, ALLOC_RECORD_SLUB inuse=Y` | 确认 pre-init 是否有遗留分配 |
| `init()` 结束 | `records=X` | 确认 reinit 后链表为空 |
| `get_record()` | `size mismatch: expected=X iterated=Y` | 检测链表损坏（D_size 与实际节点数不一致） |
| `remove_record()` | `record=X NOT found in list` | 检测节点未在链表中却被释放 |
| `free()` 重入 | `REENTRANCY detected!` | 检测中断重入（最终确认根因） |

### 日志分析速查

| 日志内容 | 含义 |
|---------|------|
| `records=0, inuse=0` | 干净启动，pre-init 无分配 |
| `size mismatch: expected=964 iterated=963` | 差值 1 = 一个节点断链（next=NULL），D_size 计数偏大 |
| `remove_record: NOT found` | `get_record` 找到了但 `_free` 期间被内层操作删了 |
| `REENTRANCY detected!` | **根因确认：中断重入** |
| `crash addr=0xd` (=13) | VIrqResource 的 `_virq` 字段值泄漏到 SLUB freelist |

---

## 所有修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `kernel/mem/slub.h` | `MixedSizeAllocator::init()` — 显式析构 + placement new |
| `kernel/mem/slub.h` | `FixedSizeAllocator::init()` — 显式析构 + placement new |
| `kernel/mem/slub.h` | `malloc()` / `free()` — 加 `IrqGuard` 防中断重入 |
| `kernel/mem/slub.h` | `get_record()` / `free()` / `remove_record()` / `init()` — 诊断日志 |
| `kernel/mem/vma.cpp` | `~TaskMemoryManager()` — `pop_front()` 替代 `for-range` |
| `kernel/object/endpoint.cpp` | `~EndpointPayload()` — 先处理 pending_sends 再 messages |
| `kernel/object/endpoint.cpp` | `~EndpointPayload()` / `~ReplyPayload()` — delete 前清空 cancel callback |

---

## 经验教训

1. **「LGA 能跑 ≠ 代码没问题」**：no-op `free()` 是最大的 bug 掩体。所有 use-after-free、double-free、重入问题都会被掩盖。
2. **placement new 必须搭配显式析构**：直接覆写有状态的 C++ 对象会导致悬垂指针和计数器错乱。
3. **分配器的中断安全性是硬需求**：任何可能在中断上下文调用的 `malloc`/`free` 都需要关中断保护。中断在链表操作中途插入 = 链表损坏。
4. **侵入式链表遍历时不能 delete 当前节点**：`for-range` 的 `++it` 依赖当前节点的 `next` 指针，delete 后该指针已无效。
