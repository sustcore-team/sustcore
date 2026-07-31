# 容器

taycpplib 的容器面向无异常内核代码。动态变体使用 allocator，静态变体
在对象内部保存存储；两者尽可能共享算法核心。

## 分类

| 类别 | 主要类型 | 说明 |
| --- | --- | --- |
| 连续序列 | `array_list`、`static_vector`、`array`、`static_array`、`array_view` | 连续内存与随机访问 |
| 字符串 | `string`、`string_view` | 拥有/借用字符序列 |
| 哈希容器 | `hash_map`、`basic_hash_set`、`hash_set`、`static_hash_set` | 平均常数时间查找 |
| 扁平关联容器 | `basic_flat_map/set`、`flat_map/set` | 有序连续存储，二分查找 |
| 位图与队列 | `basic_bitmap`、`bitmap`、`static_bitmap`、`basic_fifo`、`fifo`、`static_fifo`、`byte_fifo` | 存储策略共享核心 |
| 稳定句柄 | `slot_map`、`static_slot_map` | 代际句柄，稠密迭代 |
| 侵入式容器 | `intrusive_list`、`intrusive_tree` | 节点嵌入对象，不拥有对象 |

## 通用错误约定

- 容量不足：静态容器通常返回 `OVERFLOW_ERROR`。
- 动态分配失败：通常返回 `OUT_OF_MEMORY`，尺寸乘法溢出返回
  `ALLOCATION_SIZE_OVERFLOW`。
- 越界或无效句柄：返回 `OUT_OF_RANGE`。
- 空 FIFO 弹出：返回 `UNDERFLOW_ERROR`。

动态容器的便利构造函数可能在分配失败时 panic；需要显式恢复时优先使用
`try_create()`、`reserve()` 及返回 `expected` 的修改操作。

进一步阅读：

- [连续容器与字符串](sequences.md)
- [关联容器](associative.md)
- [位图、FIFO 与 slot map](specialized.md)
- [侵入式容器](intrusive.md)
