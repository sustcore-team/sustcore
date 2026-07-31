# 排序、二分搜索与堆

## `sort`

`tay::sort(first, last, comp, proj)` 与 range 重载使用 introsort：快速排序分区
为主，递归深度过大时退化为 heap sort，小分段使用 insertion sort。因此
平均 O(n log n)，并避免快速排序的最坏递归行为。它要求 random-access
iterator，排序不稳定。

`comp(proj(a), proj(b))` 决定顺序。Projection 使结构体可按字段排序，无需
编写只接受完整对象的比较器：

```cpp
struct task { int priority; };
tay::sort(tasks, std::ranges::less{}, &task::priority);
```

## 二分搜索

`lower_bound`、`upper_bound`、`binary_search`、`equal_range` 接受 forward
iterator/range、查询值、Compare 和 Proj。输入必须已经按同一 comp/proj
排序。随机访问范围比较次数和移动均为对数级；仅 forward iterator 时比较
仍为对数级，但迭代器前进总量可为线性。

- `lower_bound`：首个“不小于 value”的位置。
- `upper_bound`：首个“大于 value”的位置。
- `binary_search`：是否存在等价元素。
- `equal_range`：返回 `[lower, upper)`。

## 二叉堆

`make_heap`、`push_heap`、`pop_heap`、`is_heap` 要求 random-access range。
默认 `std::ranges::less` 形成 max heap，首元素为最大值；自定义 Compare 可
形成 min heap。它们同样按 `comp(proj(a), proj(b))` 工作。

- `make_heap`：在线性时间内建立堆。
- `push_heap`：调用前新元素已追加在范围末尾，复杂度 O(log n)。
- `pop_heap`：把堆顶交换到末尾，并恢复前 `n-1` 个元素的堆，O(log n)。
- `is_heap`：线性验证整个范围。

所有这些入口原地重排元素，不进行动态分配。
