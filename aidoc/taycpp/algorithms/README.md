# 算法

tay 算法使用函数对象形式导出，如 `tay::sort`、`tay::lower_bound`。多数算法
同时接受 iterator/sentinel 和 range，并支持状态化 Compare、Predicate 与
Projection。

## 头文件

| 头文件 | 内容 |
| --- | --- |
| `<tay/algobase.h>` | copy/move/fill/swap 等基础算法 |
| `<tay/algo/find.h>` | find、find_if、contains 等查找 |
| `<tay/algo/misc.h>` | reverse、rotate 等杂项算法 |
| `<tay/algo/sort.h>` | sort 与排序相关入口 |
| `<tay/algo/binary_search.h>` | lower/upper/binary/equal_range |
| `<tay/algo/heap.h>` | make/push/pop/is_heap |
| `<tay/algo/ranges.h>` | begin/end/size 等 range CPO 与概念别名 |
| `<tay/algo/cmp.h>` | 基础比较函数对象 |

调用者必须满足算法要求的迭代器类别和比较器不变量；违反严格弱序属于调用者
错误。详见 [排序、二分与堆](search-sort-heap.md)。
