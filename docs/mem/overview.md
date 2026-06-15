# Memory Subsystem Overview

Sustcore 的内存子系统分为四层:

1. 物理内存布局: 从架构/FDT 获取可用区间与保留区间。
2. 页框分配: 通过 `GFP` 暴露统一接口，设计上由 buddy 管理空闲物理页。
3. 内核堆/对象分配: 通过 `Allocator`、`KOP<T>` 和 SLUB 分配小对象或大对象。
4. 虚拟地址空间: 通过 SV39 页表、`TaskMemoryManager`、`VMA` 和 Memory capability 建立用户态映射。

## 核心关系

```text
FDT / symbols
  -> MemRegion[]
      -> GFP
          -> BuddyAllocator
              -> physical pages
          -> SLUB / Allocator / KOP
              -> kernel objects

TaskMemoryManager
  -> PageMan(SV39 root)
  -> VMA list
      -> MemoryPayload
          -> lazily allocated PhyPage records
```

## 分配路径

按 buddy + slub 模型，典型分配路径如下:

- 页表页、用户数据页、内核栈页: `GFP::get_free_page()`。
- 小型内核对象: `Allocator::malloc()` 或对象自己的 `KOP<T>`。
- 固定大小对象缓存: `slub::SlubAllocator<T>`。
- 大对象: SLUB 大对象路径或 `Allocator` 大对象路径转交 `GFP`。
- 用户内存: `MemoryPayload::ensure_page()` 懒分配物理页，随后由 `TaskMemoryManager::on_np()` 映射进页表。

## 页表模型

当前架构配置为 RISC-V 64 SV39:

- `PageMan = Riscv64SV39PageMan`

SV39 页表管理器支持:

- 4K/2M/1G 映射。
- 页表查询。
- 单页和范围映射。
- 单页和范围解除映射。
- 权限位修改。
- 页表根切换。
- TLB 刷新。
- PTE 软件位标记 COW。
- 页表合并: `merge_from()` 递归把另一棵页表的有效映射并入当前页表。

## 用户地址空间模型

用户地址空间不直接拥有物理页。它通过 `VMA` 指向 `MemoryPayload`:

- `VMA` 记录虚拟区间、权限、增长方式、Memory 偏移和加载状态。
- `MemoryPayload` 记录承诺大小和已经实际分配的 `PhyPage`。
- 缺页时根据 fault 地址定位 VMA，再按 Memory 偏移懒分配并映射物理页。

这种设计让 Memory capability、VMA、页表三者分工明确:

- Capability 决定“是否允许映射、读、写、执行、调整大小”。
- VMA 决定“映射到哪个虚拟区间，用什么页权限”。
- PageMan 决定“硬件页表中如何表示映射”。

## 当前限制

- 页表析构路径中仍有 `TODO: 释放页表`，`TaskMemoryManager` 目前主要解除用户映射并刷新 TLB。
- COW 处理只支持 4K 页；遇到大页会返回 `NOT_SUPPORTED`。
- 用户访问辅助通过 SUM 或临时页表切换访问用户地址，调用方必须保证地址范围已经由 VMA/页表覆盖。
- 新任务地址空间初始化会继承主内核页表映射，因此 KVA/KPA/MMIO 在各任务页表中都存在。
- buddy/slub 作为主路径时，需要确保 `RawGFPImpl` 和 `Allocator` 别名已经切换，否则释放/复用行为会退化到线性分配器语义。
