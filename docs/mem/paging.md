# SV39 Paging

本文说明 RISC-V 64 SV39 页表管理器。相关实现位于 `kernel/arch/riscv64/mem/sv39.h` 和 `kernel/arch/riscv64/mem/sv39.cpp`。

## 类型别名

架构描述文件中定义:

```cpp
using PageMan = Riscv64SV39PageMan;
```

当前内核 SV39 管理器统一通过 KPA 线性映射访问页表页。

## 页大小

SV39 管理器支持三种页大小:

- 4K
- 2M
- 1G

`map_range<true>()` 会按地址对齐和剩余页数优先使用 1G 页，再使用 2M 页，最后使用 4K 页。

## PTE 结构

`PTE` 是 64 位页表项，关键字段:

- `v`: Valid。
- `rwx`: R/W/X 权限位。
- `u`: User 可访问位。
- `g`: Global 位。
- `a`: Accessed 位。
- `d`: Dirty 位。
- `rsw`: 软件保留位。
- `ppn`: 物理页号。
- `np`: Not Present 位。

当前 COW 使用 `rsw` 的低位标记:

```cpp
PageMan::set_cow(pte, true);
PageMan::is_cow(pte);
```

## RWX 权限

`Riscv64SV39RWX` 表示页权限:

- `P`: 指向下一级页表，不是叶子映射。
- `RO`
- `RW`
- `RX`
- `RWX`
- `NONE`

`PageMan::rwx(r, w, x)` 可从布尔权限构造枚举。

`PageMan::without_write(rwx)` 用于 COW 写保护，把 `RW` 降为 `RO`，把 `RWX` 降为 `RX`。

## 查询页

`query_page(vaddr)` 从根页表开始按 VPN 逐级查找:

1. PTE 无效时返回 `PAGE_NOT_PRESENT`。
2. PTE 有效且 `rwx != P` 时认为找到叶子映射。
3. 根据所在层级判断映射大小: 1G、2M 或 4K。
4. 到达最后一级仍是 `P` 时返回 `INVALID_PTE`。

返回值:

```cpp
struct QueryResult {
    PTE *pte;
    PageSize size;
};
```

## 映射页

`map_page<size>(vaddr, paddr, rwx, u, g)` 按目标页大小建立映射。

如果中间页表不存在，会通过 `GFP::get_free_page(1)` 分配新的页表页，并清零。中间节点 PTE 使用 `RWX::P`，叶子 PTE 写入目标物理地址、权限、U/G 位。

当前接口在重复映射、有效但 `np` 的异常情况中记录日志并返回，不通过 `Result` 上报错误。因此调用方需要避免重复映射。

## 范围映射与解除映射

`map_range<use_hugepage>(vstart, pstart, size, rwx, u, g)`:

- 会将起始地址向下页对齐。
- 会将大小向上页对齐。
- `use_hugepage=false` 时逐 4K 页映射。
- `use_hugepage=true` 时优先 1G/2M/4K。

`unmap_page(vaddr)` 查询页表后把对应 PTE 清零。

`unmap_range(vstart, size)` 逐 4K 地址调用 `unmap_page()`。如果某个地址命中大页，解除行为仍由 `query_page()` 返回的叶子 PTE 决定。

## 页表映射复制与合并

`clone_mapping_from(src, vaddr)` 从另一个页表中查询 `vaddr` 对应的叶子映射，并在当前页表中建立同大小、同权限、同 U/G 位的映射。它常用于缺页路径中把主内核页表的某个内核映射补到当前页表。

`merge_from(src)` 会递归遍历 `src` 的整棵页表，并把所有有效映射并入当前页表:

- 源 PTE 无效时跳过。
- 目标 PTE 为空时复制源映射；如果源 PTE 指向下级页表，则先为目标分配并清零下级页表，再继续递归。
- 目标 PTE 已有效时保持目标现状，不覆盖已有映射。
- 当源和目标同为下级页表节点时继续递归合并。

该接口用于让新任务页表继承主内核页表中的完整内核映射，包括 KVA、KPA 和 MMIO 相关映射；它不是通用的冲突解析器。

## 修改权限

`ModifyMask` 控制修改哪些位:

- `R`
- `W`
- `X`
- `U`
- `G`
- `NP`
- `RWX`
- `ALL`

`modify_flags<mask>(vaddr, rwx, u, g)` 修改单页。

`modify_range_flags<mask>(vstart, size, rwx, u, g)` 修改范围。它会根据查询结果的页大小跳过对应跨度，因此兼容大页映射。

## 页表根和 TLB

`read_root()` 从 `satp` 读取当前 SV39 根页表物理地址。

`make_root(root)` 清零一页作为页表根。

`switch_root()` 和 `__switch_root(root)` 写入 `satp`:

- `mode = SV39`
- `asid = 0`
- `ppn = root >> 12`

`flush_tlb()` 执行 `sfence.vma`。

## COW 支持

页表层 COW 只负责两件事:

- 把可写映射降权为只读。
- 在 PTE `rsw` 位中标记 COW。

真正的物理页拆分由 `MemoryPayload::fork()` 完成，页表在 `TaskMemoryManager::on_wp()` 中更新为新物理页并恢复原 VMA 权限。

## 当前限制

- 页表页释放尚未完整实现。
- COW 暂不支持大页。
- `map_page()` 遇到错误只记录日志，不返回 `Result`。
- `merge_from()` 不覆盖目标页表中的已有有效映射。
- ASID 暂未使用，`satp.asid` 固定为 0。
