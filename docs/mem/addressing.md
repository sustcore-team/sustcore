# Addressing Model

本文说明 Sustcore 的地址类型、地址范围和转换规则。相关实现位于 `include/sustcore/addr.h`。

## 地址类型

内核使用强类型地址封装，避免裸整数在不同地址空间之间混用:

- `PhyAddr`: 物理地址。
- `KpaAddr`: 内核物理映射地址，位于 KPA 区间。
- `KvaAddr`: 内核虚拟地址，位于 KVA 高半区。
- `VirAddr`: 任意虚拟地址，常用于用户地址或通用虚拟地址。

`Addr<Type>` 内部保存 `addr_t`，提供:

- `addr()`: 转为 `void *`。
- `arith()`: 转为整数。
- `as<T>()`: 转为 `T *`。
- `nonnull()`
- `aligned<N>()` / `aligned(size)`
- `align_up()` / `align_down()`
- `page_align_up()` / `page_align_down()`
- 地址加减和比较。

## 地址范围

当前常量:

```cpp
KVA_OFFSET = 0xFFFF'FFFF'0000'0000
KPA_OFFSET = 0xFFFF'FFC0'0000'0000
```

地址范围:

- `KVA_SCOPE`: `[KVA_OFFSET, MAX_ADDR]`
- `KPA_SCOPE`: `[KPA_OFFSET, KVA_OFFSET - 1]`
- `PA_SCOPE`: `[0, KPA_OFFSET - 1]`
- `VADDR_SCOPE`: `[0, MAX_ADDR]`

`within_scope(addr, type)` 用于检查地址是否属于指定范围。空地址 `0` 对所有类型都视为合法空值。

## 转换规则

`convert<T>(addr)` 支持 `PhyAddr`、`KpaAddr`、`KvaAddr` 之间转换:

```cpp
KpaAddr kpa = convert<KpaAddr>(paddr);
PhyAddr pa  = convert<PhyAddr>(kpa);
KvaAddr kva = convert<KvaAddr>(paddr);
```

转换本质依赖固定 offset:

- `PA2KVA(pa) = pa + KVA_OFFSET`
- `KVA2PA(kva) = kva - KVA_OFFSET`
- `PA2KPA(pa) = pa + KPA_OFFSET`
- `KPA2PA(kpa) = kpa - KPA_OFFSET`

`convert_pointer(ptr)` 会根据指针值所在范围判断其当前地址类型，再转换为 `PhyAddr`。它常用于 linker symbol 或早期环境指针。

## 用户地址判断

`is_user_vaddr(VirAddr vaddr)` 判断地址是否在通用虚拟地址范围内，且不属于 KVA/KPA 区间。

VMA 创建时会使用该规则验证用户虚拟区间:

```cpp
is_user_vaddr(varea.begin) && is_user_vaddr(varea.end)
```

## 区间类型

地址区间使用 `util::range::Range<T>`:

- `VirArea = Range<VirAddr>`
- `PhyArea = Range<PhyAddr>`

它们采用半开区间 `[begin, end)`，用于 VMA、物理内存区域、内核段和设备资源。

## 注意事项

- 不要把 `PhyAddr`、`KpaAddr`、`KvaAddr` 随意转成整数再手动加 offset，优先使用 `convert<>()`。
- `VirAddr` 是通用虚拟地址，不保证一定是用户地址；判断用户地址需使用 `is_user_vaddr()`。
- 页表中保存的是物理页号，访问页表页内容时必须先转换为可访问的 KPA 地址。
- KPA 与 KVA 都是内核可访问地址，但语义不同: KPA 更强调“物理内存线性映射”，KVA 更强调“内核镜像和高半区虚拟地址”。
