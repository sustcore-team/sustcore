# Task Memory Manager

本文总结任务地址空间管理。核心实现位于 `kernel/mem/vma.*`，由 `kernel/task/task.cpp` 中的进程创建、fork、exec 路径持有和调用；RISC-V 的硬件页表操作由 `kernel/arch/riscv64/mem/sv39.*` 提供。

## 总体模型

每个用户进程的 `PCB` 持有一个:

```cpp
util::owner<TaskMemoryManager *> tmm;
```

`TaskMemoryManager` 维护:

- VMA 链表。
- 页表根物理地址 `_pgd`。
- 架构页表管理器 `PageMan _pman`。

新建 `TaskMemoryManager` 时会:

1. 清空页表根。
2. 映射内核区域。

这保证每个用户地址空间都有独立用户映射，同时共享内核高地址映射。

## VMA

`VMA` 表示一段用户虚拟地址区域，所有 VMA 都必须绑定 `cap::MemoryPayload`。

主要字段:

- `type`: VMA 类型。
- `growth`: 允许的增长/收缩方向。
- `tm`: 所属 `TaskMemoryManager`。
- `memory`: 关联 Memory payload。
- `rwx`: 页权限。
- `mem_offset`: VMA 起点对应的 Memory 内偏移。
- `varea`: 虚拟地址范围。
- `loading`: 是否处于 ELF 加载阶段。

VMA 构造时会 `memory->keep()`，析构时会 `memory->release()`。

### VMA 类型

当前类型包括:

- `CODE`
- `DATA`
- `STACK`
- `HEAP`
- `SHARE_RW`
- `SHARE_RO`
- `SHARE_RX`
- `SHARE_RWX`

`VMA::seg2rwx()` 把 VMA 类型转换为页权限:

- `CODE` -> `RX`
- `DATA` / `STACK` / `HEAP` / `SHARE_RW` -> `RW`
- `SHARE_RO` -> `RO`
- `SHARE_RX` -> `RX`
- `SHARE_RWX` -> `RWX`

## Memory payload

`cap::MemoryPayload` 表示一段承诺大小的物理内存对象。

它保存:

- `memsz`: 承诺大小。
- `shared`: clone 时是否共享同一 payload。
- `continuity`: 是否要求物理连续。
- `growth`: 允许增长/收缩方式。
- `phy_pages`: 已实际分配的物理页。

物理页按需分配。VMA 只保存地址范围和权限；实际页在读写 payload 或处理缺页时才分配。

## 添加和定位 VMA

`add_vma(type, growth, varea, memory, rwx, mem_offset)` 会:

1. 检查 memory 非空。
2. 检查虚拟区域是用户地址范围。
3. 拒绝与已有 VMA 相交。
4. 构造 VMA 并挂入链表。

定位接口:

- `locate(vaddr)`: 找到覆盖某地址的 VMA。
- `locate_range(varea)`: 找到与某范围相交的 VMA。
- `locate_memory(memory, vaddr)`: 找到指定 memory 在某地址处的 VMA。

## 懒分配缺页

RISC-V trap 处理器发现 `NO_PRESENT` 用户页异常时，会调用当前环境中的 `TaskMemoryManager::on_np()`。

处理流程:

1. 用 fault address 定位 VMA。
2. 将地址向下页对齐。
3. 根据 VMA 起点和 `mem_offset` 计算 Memory 内偏移。
4. 调用 `memory->ensure_page()` 懒分配物理页。
5. 查询物理页地址。
6. 根据 `loading` 和 VMA 权限决定最终页权限。
7. 若该页需要 COW，先映射为去写权限并设置 COW 标记。
8. 调用 `PageMan::map_page<_4K>()` 建立页表项。
9. 刷新 TLB。

加载 ELF 时，VMA 的 `loading` 为 true。此时页面按内核页处理，加载完成后再修改为用户页权限。

## 写时复制

COW 路径分两部分。

### fork 时保护

`TaskMemoryManager::clone_to_cow(dst)` 会:

1. 遍历父进程 VMA。
2. 调用 `MemoryPayload::clone_payload()` 克隆或共享 payload。
3. 在子地址空间添加同样的 VMA。
4. 克隆已存在页表映射。
5. 对非 shared 且可写的页，父子页表都清除写权限并设置 COW 标记。

`MemoryPayload::clone_payload()` 对非 shared memory 会:

- 创建新的 payload。
- 对已有物理页增加 GFP 引用。
- 增加 payload 视角下的 page refcount。
- 把相同物理页记录插入克隆 payload。

### 写保护异常

写 COW 页触发 `WRITE_PROTECT` 时，trap 处理器调用 `TaskMemoryManager::on_wp(fault_addr)`:

1. 定位 VMA。
2. 拒绝 shared memory。
3. 查询页表项。
4. 要求是 4K 页且带 COW 标记。
5. 调用 `MemoryPayload::fork(mem_offset)` 拆分物理页。
6. 将 PTE 物理地址改成新页。
7. 恢复 VMA 原始写权限。
8. 清除 COW 标记并刷新 TLB。

## 与 SV39 页表的关系

`PageMan` 是 `Riscv64SV39PageMan`。

关键能力:

- `make_root(root)`: 初始化页表根。
- `query_page(vaddr)`: 查询 PTE 和页大小。
- `map_page<PageSize>()`: 建立 4K/2M/1G 映射。
- `map_range<use_hugepage>()`: 映射范围。
- `unmap_page()` / `unmap_range()`
- `modify_range_flags<mask>()`: 修改权限位。
- `clone_mapping_from(src, vaddr)`: 从其它页表复制同一映射。
- `switch_root()`: 写 `satp`。
- `flush_tlb()`: 执行 `sfence.vma`。

调度器切换线程时会调用 `schd::switch_pgd(tmm)`，在页表根不为空且和当前环境页表不同的时候切换 `satp` 并刷新 TLB。

## Memory capability 映射

`cap::MemoryObject::map_into(tmm, vaddr, rwx, growth)` 是用户通过 capability 把 Memory 映射进地址空间的入口。

权限要求:

- 必须有 `perm::memory::MAP`。
- 请求页权限必须可读。
- 必须有 `perm::memory::READ`。
- 请求可写时需要 `perm::memory::WRITE`。
- 请求可执行时需要 `perm::memory::EXEC`。
- 请求向上增长/收缩时需要 `FLEXUP`。
- 请求向下增长/收缩时需要 `FLEXDOWN`。

还会检查请求 growth 是否被 payload 自身允许。

成功后，会在目标 TMM 中创建共享类 VMA。当前实现固定使用 `VMA::Type::SHARE_RW`，页权限来自调用方传入的 `rwx`。

## 用户栈和堆

ELF 加载器会根据所有 PT_LOAD 段末尾向上页对齐创建初始堆:

- VMA 类型: `HEAP`
- growth: `FLEXUP`
- 初始范围为空 `[heap_start, heap_start)`
- heap memory cap 插入进程 holder

主线程创建时会创建初始栈:

- VMA 类型: `STACK`
- growth: `GROW_DOWN`
- 范围: `[USER_STACK_BOTTOM, USER_STACK_TOP)`
- stack memory cap 插入进程 holder

## 当前限制

当前地址空间管理仍有一些限制:

- 页表中间层释放仍有 TODO。
- COW 只支持 4K 页，不支持大页。
- `valid_user_area()` 要求 begin/end 都是用户地址，内核映射不通过 VMA 管理。
- VMA 列表是线性查找。
- shared memory 不走 COW。
- `MemoryObject::map_into()` 的 VMA type 当前没有按 RO/RX/RWX 细分。
