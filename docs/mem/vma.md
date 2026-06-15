# Task Address Space And VMA

本文说明任务地址空间、VMA、缺页处理和 COW。相关实现位于 `kernel/mem/vma.*` 和 `kernel/object/memory.*`。

## `TaskMemoryManager`

`TaskMemoryManager` 管理一个任务的地址空间:

- `vma_list`: 该地址空间中的 VMA 侵入式链表。
- `_pgd`: 页表根物理地址。
- `_pman`: 对 `_pgd` 的 SV39 页表管理器。

构造新地址空间时:

```cpp
PageMan::make_root(_pgd);
PageMan kernel_pman(env::inst().main_kernel_pgd());
_pman.merge_from(kernel_pman);
```

因此每个任务页表默认继承主内核页表中的全部内核映射，包括 KVA、KPA 和 MMIO 映射。用户区映射由 VMA 和缺页处理懒建立，且 `merge_from()` 不覆盖目标页表中已有的有效映射。

`TaskMemoryManager::from_existing_pgd()` 用于包装已有页表根，不会主动清零或合并页表内容；内核进程通过它绑定主内核页表。

## `VMA`

`VMA` 表示一个用户虚拟地址区间:

- `type`: CODE/DATA/STACK/HEAP/SHARE_*。
- `growth`: 允许增长/收缩方式。
- `tm`: 所属 `TaskMemoryManager`。
- `memory`: 关联的 `MemoryPayload`。
- `rwx`: 页权限。
- `mem_offset`: VMA 起点对应的 Memory 内偏移。
- `varea`: 虚拟地址半开区间。
- `loading`: ELF 加载等特殊阶段标志。

所有 VMA 都必须关联 `MemoryPayload`。VMA 构造时会 `memory->keep()`，析构时 `memory->release()`。

## VMA 类型和权限

`VMA::seg2rwx(type)` 把段类型转换为页权限:

- `CODE`: `RX`
- `DATA` / `STACK` / `HEAP`: `RW`
- `SHARE_RW`: `RW`
- `SHARE_RO`: `RO`
- `SHARE_RX`: `RX`
- `SHARE_RWX`: `RWX`

`VMA::sharable(type)` 判断是否是 shared 类型。

`VMA::cowable(type)` 判断是否适合 COW，当前 CODE/DATA/STACK/HEAP 可 COW。

## 添加和定位 VMA

`add_vma()` 会检查:

- `memory != nullptr`
- 地址范围是用户地址。
- 新范围不与已有 VMA 相交。

成功后创建 `VMA` 并插入 `vma_list`。

定位接口:

- `locate(vaddr)`: 找覆盖某地址的 VMA。
- `locate_range(varea)`: 找与范围相交的 VMA。
- `locate_memory(memory, vaddr)`: 找指定 Memory 在某地址处的 VMA。

## 懒分配与缺页处理

创建 VMA 不会立即分配物理页。缺页时 `TaskMemoryManager::on_np()` 处理:

1. 根据 fault 地址定位 VMA。
2. 将 fault 地址向下页对齐。
3. 计算该页对应的 Memory 内偏移。
4. 调用 `memory->ensure_page(offset)` 懒分配物理页。
5. 根据 VMA 权限和 COW 状态决定实际映射权限。
6. 调用 `_pman.map_page<4K>()` 建立页表映射。
7. 必要时设置 PTE COW 标记。
8. 刷新 TLB。

`loading=true` 时，缺页映射使用 `RW` 且 `u=false`，用于 ELF 加载阶段由内核写入数据。加载完成后按 VMA 权限映射用户页。

## VMA 增长和收缩

`grow_vma(vma, new_area)` 支持四种方向:

- grow up: begin 不变，end 增大。
- shrink up: begin 不变，end 减小。
- grow down: end 不变，begin 减小。
- shrink down: end 不变，begin 增大。

它会检查 VMA 的 `growth` 位是否允许该操作，并检查新区域是否与其它 VMA 冲突。

收缩时会解除被移除区间中的页表映射，但物理页生命周期仍由 `MemoryPayload` 管理。

## COW clone

`clone_to_cow(dst)` 用于 fork:

1. 遍历父地址空间 VMA。
2. 调用 `vma.memory->clone_payload()` 克隆或共享 Memory payload。
3. 在子地址空间添加同样的 VMA。
4. 对已映射页面执行 `clone_vma_pages_to_cow()`。

非 shared Memory 的已映射可写页会:

- 父 PTE 去掉写权限。
- 子 PTE 映射同一物理页且去掉写权限。
- 父子 PTE 都设置 COW 标记。
- `MemoryPayload::clone_payload()` 中增加 GFP 引用计数和 payload 内页引用计数。

## 写保护异常

`on_wp(fault_addr)` 处理 COW 写保护:

1. 定位 VMA。
2. 检查 Memory 非 shared。
3. 查询页表项。
4. 要求命中 4K 页。
5. 检查 PTE 是 COW 页。
6. 调用 `memory->fork(offset)` 拆分物理页。
7. 更新 PTE 指向新物理页。
8. 恢复 VMA 的原始 `rwx`。
9. 清除 COW 标记并刷新 TLB。

## 与 Memory payload 的分工

VMA 只管理虚拟区间和页表映射，不拥有物理页。物理页由 `MemoryPayload` 的 `phy_pages` 记录管理:

- `ensure_page()` 懒分配。
- `lookup_page()` 查询。
- `fork()` 写时复制。
- `resize()` 调整承诺大小。
- `destruct()` 释放所有物理页。

## 当前限制

- `TaskMemoryManager` 析构中页表页释放仍是 TODO。
- COW 不支持大页。
- VMA 地址合法性要求 begin/end 都是用户地址；调用方应避免传入 KVA/KPA。
- 缺页处理假设 fault 已由当前任务页表触发，跨地址空间操作需使用用户访问辅助。
- 新建用户地址空间依赖 `env::inst().main_kernel_pgd()` 已初始化，因此任务管理器初始化必须晚于正式内核页表建立。
