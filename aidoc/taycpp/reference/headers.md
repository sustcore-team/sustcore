# 公开头文件索引

## taycpplib

| 类别 | 头文件 |
| --- | --- |
| 算法 | `tay/algobase.h`、`tay/algo/{cmp,find,misc,ranges,sort,binary_search,heap}.h` |
| 连续容器 | `tay/array_list.h`、`tay/static_vector.h`、`tay/array.h` |
| 专用容器 | `tay/bitmap.h`、`tay/fifo.h`、`tay/slot_map.h` |
| 关联容器 | `tay/map.h`、`tay/set.h`、`tay/flat.h` |
| 侵入式容器 | `tay/intrusive.h`、`tay/list.h`、`tay/tree.h`、`tay/container_of.h` |
| 字符串 | `tay/string.h`、`tay/string_view.h` |
| RAII/所有权 | `tay/raii.h`、`tay/guard.h`、`tay/lock.h`、`tay/owner.h`、`tay/unique_ptr.h`、`tay/refcount.h` |
| 原子同步 | `tay/spinlock.h` |
| 错误/分配 | `tay/expected.h`、`tay/err.h`、`tay/panic.h`、`tay/allocator.h` |
| callable/utility | `tay/functional.h`、`tay/utility.h` |
| 服务 | `tay/format.h`、`tay/fmt/*.h`、`tay/logger.h`、`tay/path.h`、`tay/units.h` |
| 其他 | `tay/range.h`、`tay/rtti.h`、`tay/reflection.h` |

## 其他库

- tayclib：`tay/ansi.h`、`tay/attribute.h`、`tay/bits.h`、`tay/bool.h`、
  `tay/itoa.h`、`tay/macros.h`。
- mini-cstd：`assert.h`、`ctype.h`、`stdbool.h`、`stdlib.h`、`string.h`、
  `uchar.h`、`wchar.h` 及 `feature/*`。
- mini-cppstd：参见 [mini-cppstd](../runtime/cpp-runtime.md) 的头文件族。

内部路径 `tay/detail/*` 和 `tay/__algo` 实现命名空间不属于稳定公开 API。
