# 连续容器与字符串

## `array_list<T, Allocator>`

头文件：`<tay/array_list.h>`。

动态、连续、可增长序列，角色接近 `std::vector`。它提供迭代器、
`data/size/capacity`、`reserve`、`resize`、`push_back/emplace_back`、
插入、擦除和 `clear` 等接口。可能分配或越界的修改接口返回
`expected`。扩容会使指针、引用和迭代器失效；中部插入/擦除会移动后续
元素。

## `static_vector<T, N>`

头文件：`<tay/static_vector.h>`。

最大容量为 `N` 的内联连续序列，不进行堆分配。对象只构造 `[0, size())`
范围内的元素，因此可保存不可默认构造的类型。API 与 `array_list` 的常用
部分对齐；超过容量的操作返回 `OVERFLOW_ERROR`。元素地址不会因扩容而
变化，但中部插入/擦除仍会移动元素。

```cpp
tay::static_vector<int, 4> values;
values.emplace_back(1);
values.push_back(2);
```

## array storage family

头文件：`<tay/array.h>`。

`basic_array<T, N, Storage>` 保存固定长度数组算法，三个公开变体只改变
存储语义：

| 类型 | 所有权 | 存储位置 |
| --- | --- | --- |
| `array<T, N, Allocator>` | 拥有 | 动态分配固定 `N` 个元素 |
| `static_array<T, N>` | 拥有 | 对象内联保存 `N` 个元素 |
| `array_view<T, N>` | 不拥有 | 借用外部连续内存；`N` 可为 `dynamic_extent` |

它们共享 `begin/end/data/size/empty/operator[]/at/front/back/fill`。`at()`
返回 `expected<reference, error_code>`。固定 extent 的 view 构造时会验证
长度，不匹配会 panic。view 不延长底层对象生命周期。

## `string` 与 `string_view`

头文件：`<tay/string.h>`、`<tay/string_view.h>`。

`string_view` 是只读借用视图，支持迭代、比较、`substr`、前后缀判断以及
`find/rfind/find_*_of` 等搜索；`npos` 表示未找到。需要验证位置的操作如
`at`、`remove_prefix`、`substr` 返回 `expected`。

`string` 拥有可变字符缓冲区并使用 allocator。它提供构造/赋值、容量管理、
追加、插入、擦除、替换与查询。动态失败通过 `expected` 或便利入口的 panic
表现。任何重新分配都会使原有 view、指针和迭代器失效。

`string_view(const char*)` 要求 NUL 结尾；`string_view(nullptr)` 被删除。
