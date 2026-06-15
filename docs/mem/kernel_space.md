# Kernel Address Space

本文说明内核地址空间布局和映射方式。相关实现位于 `kernel/mem/kaddr.*` 和 `kernel/main.cpp`。

## 内核段

`ker_paddr::Segment` 描述一段物理地址和虚拟地址的对应关系:

```cpp
struct Segment {
    PhyAddr pstart, pend;
    VirAddr vstart, vend;
};
```

当前维护的段:

- `kernel`
- `text`
- `rodata`
- `data`
- `bss`
- `misc`

这些段由 linker symbols 构造:

- `skernel` / `ekernel`
- `s_text` / `e_text`
- `s_rodata` / `e_rodata`
- `s_data` / `e_data`
- `s_bss` / `e_bss`
- `s_misc`

## KVA 段

`make_kva_seg()` 把内核镜像物理区间映射到 `KVA_OFFSET` 高半区:

```cpp
VirAddr vs = ps + KVA_OFFSET;
VirAddr ve = pe + KVA_OFFSET;
```

内核代码、只读数据、数据段、BSS 和 misc 区域都通过这种方式构造。

## KPA 线性物理映射

KPA 线性映射不再作为 `ker_paddr::Segment` 维护。正式内核页表建立阶段由 `main.cpp` 中的 `map_kpa_region()` 显式映射物理区间:

```cpp
VirAddr vaddr = VirAddr(convert<KpaAddr>(parea.begin).arith());
man.map_range<true>(vaddr, parea.begin, parea.size(), PageMan::RWX::RW,
                    false, true);
```

当前会映射:

- SBI 可回收区域。
- 所有 `FREE` 物理内存区域。
- 切换正式页表后，再映射并回收 SBI 页表保留区。

这些映射使内核可以通过 KPA 访问页表页、普通物理页和线程内核栈页。

## 映射权限

`ker_paddr::mapping_kernel_areas(man)` 映射内核段:

- `text`: `R-X`
- `rodata`: `R--`
- `data`: `RW-`
- `bss`: `RW-`
- `misc`: `R--`

这些映射都设置:

- `u = false`
- `g = true`

`g = true` 表示全局映射，适合所有地址空间共享的内核区域。

## 每个任务页表中的内核映射

`TaskMemoryManager` 构造新页表时会:

```cpp
PageMan::make_root(_pgd);
PageMan kernel_pman(env::inst().main_kernel_pgd());
_pman.merge_from(kernel_pman);
```

因此每个任务地址空间都会继承主内核页表中的 KVA、KPA 和 MMIO 映射。用户区 VMA 映射仍按需建立，并且不会覆盖已经存在的内核映射。

主内核页表根保存在 `env::inst().main_kernel_pgd()` 中。内核线程通过 `TaskMemoryManager::from_existing_pgd()` 包装这张主内核页表；用户进程则分配自己的页表根并合并主内核页表内容。

## 注意事项

- 内核段映射权限应尽量保持最小权限，尤其是 `text` 不应可写。
- KPA 映射是调试、页表访问和内核栈访问的基础，不代表用户可任意访问物理内存；相关 PTE 的 `u` 位应保持关闭。
- 新增内核段或修改 linker symbols 后，应同步检查 `ker_paddr::init()` 与映射权限。
- 新增主内核页表映射后，如果用户地址空间也需要在调度或 trap 路径中访问该映射，应确认新建 `TaskMemoryManager` 会通过 `merge_from()` 继承它。
