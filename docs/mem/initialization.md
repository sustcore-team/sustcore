# Memory Initialization

本文说明内存相关初始化顺序。主要入口位于 `kernel/main.cpp`，物理内存探测位于 `kernel/arch/riscv64/device/memory.cpp`。

## 早期入口

`kernel_setup()` 调用 `pre_init()`。早期阶段的关键步骤:

1. `env::construct()` 构造全局环境对象。
2. `Initialization::pre_init()` 执行架构早期初始化。
3. `MemoryLayout::detect()` 探测物理内存布局。
4. 遍历 `env::meminfo().regions`，计算 `uppm`。
5. `GFP::pre_init()` 初始化页框分配器。
6. `PageMan::init()` 初始化页表管理器。
7. 建立正式内核页表并切换到高半区。
8. 继续完成 post-init 阶段初始化。

## 物理内存探测

RISC-V 内存探测读取 FDT:

- `/memory` 节点的 `reg` 属性提供物理内存区间。
- `/reserved-memory/mmode_resv*` 提供 M-mode 保留区间。
- 内核镜像 `[skernel, ekernel)` 也会被加入保留区间。

探测过程会把内存区和保留区排序，然后线性扫描生成最终 `MemRegion[]`:

- `FREE`: 可交给 GFP 管理。
- `RESERVED`: 固件、内核、设备或其它不可分配区域。

探测完成后，`env::inst().meminfo()` 保存:

- `regions`
- `region_cnt`
- `lowpm`
- `uppm`
- `lowvm`

这些字段随后用于 GFP 初始化、内核页表映射和设备模型。

## 早期页表建立

`env_setup()` 中的正式内核页表建立流程如下:

1. 通过 `GFP::get_free_page(1)` 分配内核页表根。
2. 把页表根写入 `env::inst().main_kernel_pgd()`。
3. 调用 `PageMan::make_root(new_pgd)` 清零根页表。
4. 调用 `ker_paddr::mapping_kernel_areas(kernelman)` 映射 KVA 内核段。

随后它会把 KPA 物理内存区域映射进主内核页表:

```cpp
map_kpa_region(kernelman, SBI_RECLAIMABLE_AREA);
for (FREE region in meminfo.regions) {
    map_kpa_region(kernelman, region);
}
```

`map_kpa_region()` 使用 `convert<KpaAddr>(paddr)` 计算 KPA 虚拟地址，并通过 `map_range<true>()` 建立 `RW`、`u=false`、`g=true` 映射。这里 `use_hugepage=true`，页表管理器会优先使用 1G 页，其次 2M 页，最后 4K 页。

最后切换页表根并刷新 TLB:

```cpp
kernelman.switch_root();
kernelman.flush_tlb();
```

切换到正式页表后，还会把 SBI 页表保留区映射进 KPA，并交还给 GFP。

## 后初始化

`post_init()` 在内核虚拟地址空间中运行。内存相关步骤:

1. `Allocator::init()` 初始化对象分配器。
2. `init_kop()` 初始化内核对象池。
3. `cap::CHolderManager::init()` 初始化能力系统。
4. `Interrupt::init()` 初始化中断入口。
5. 构建设备模型并初始化当前 hart。
6. `task::TaskManager::init()` 初始化任务管理器和内核线程。
7. `wait::WaitReasonManager::init()` 初始化等待系统。
8. 初始化调度器并运行前置内核测试。

在 buddy + slub 模型下:

- `GFP::post_init()` 应让 buddy 空闲块链表中的节点指针从早期物理地址语义迁移到 KPA 语义。
- `Allocator::init()` 应初始化 SLUB mixed-size allocator 和各固定大小 cache。

## Buddy 的阶段迁移

`BuddyAllocator::pre_init()` 会:

- 初始化每个 order 的空闲链表头。
- 初始化外置 `FreeBlock` 池 `_buddy_pool0`。
- 遍历 `MemRegion[]`，把所有 FREE 区域按页对齐后加入 buddy。
- 额外回收 `[`&`s_sbi`, `&s_sbi_paging` `)` 这段 SBI 引导区，并加入 buddy。

`BuddyAllocator::post_init()` 会:

- 把各 order 链表头切换到 post-init 可访问地址语义。
- 打印迁移后的 buddy 布局。

这一步仍然重要，因为动态扩容出来的 `FreeBlock` 池位于物理页中；进入高半区后，需要通过 post-init 地址语义继续访问这些元数据。

## 初始化顺序约束

- `MemoryLayout::detect()` 必须早于 `GFP::pre_init()`。
- `GFP::pre_init()` 必须早于早期页表根分配。
- `ker_paddr::init()` 依赖 linker symbols 和 `env::meminfo()`。
- `env::inst().main_kernel_pgd()` 必须在创建用户 `TaskMemoryManager` 前可用；新任务页表会从主内核页表合并内核映射。
- `Allocator::init()` 必须在 post-init 后执行，因为 SLUB 依赖稳定的 GFP 与内核地址空间。
- 任务、VFS、设备模型初始化前必须完成内核地址空间和对象分配器初始化。
