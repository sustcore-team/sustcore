# 位图、FIFO 与槽位映射

## 位图存储族

头文件：`<tay/bitmap.h>`。

```cpp
basic_bitmap<Storage>
bitmap<Allocator>
static_bitmap<N>
```

动态 storage 在运行时确定 bit 数并通过 `try_create(bit_count)` 建立；静态
storage 将 bit 数 `N` 编进类型。共享操作包括单 bit 的 `test/set/reset/flip`、
全体置位/清零/翻转、`count/any/none/all`、查找首个置位/清零 bit，以及
`& | ^`。检查版单 bit 操作返回 `expected`；`operator[]` 不检查边界。
`words()` 暴露底层 `uint64_t` 数组，调用者修改后应维持尾部无效 bit 为零。

## FIFO 存储族

头文件：`<tay/fifo.h>`。

```cpp
basic_fifo<T, Storage>
fifo<T, Allocator>
static_fifo<T, N>
byte_fifo<Allocator>
```

`basic_fifo` 是环形队列核心。动态版可 `reserve`，静态版容量固定。`push`、
`emplace`、`pop`、`front/back` 均显式报告满/空错误。迭代器按逻辑 FIFO
顺序遍历，而不是按物理环形内存顺序。

`readable_segments()` 将已有数据分成至多两个 `array_view`；对 trivially
copyable 元素，`writable_segments()`/`commit()` 和 `consume()` 可实现少
复制 I/O。`byte_fifo` 在此基础上提供字节流 `try_write/try_read` 接口。

## 槽位映射

头文件：`<tay/slot_map.h>`。

```cpp
slot_map<T, Allocator>
static_slot_map<T, N>
```

插入返回 `slot_map_handle{index, generation}`。`contains/get/at` 同时验证槽
索引和代数，可识别删除后遗留的旧句柄。值在稠密数组中保存，遍历缓存友好；
擦除时可能把最后一个值移动到洞中，因此值地址和迭代器并不稳定，稳定的是
句柄语义。

`erase` 增加 generation；若 generation 回绕到零，该槽退休而不再复用。
静态版容量为 `N`，动态版使用 allocator。`get()` 失败返回空指针，`at()`、
`erase()`、`extract()` 返回 `expected`。
