# Tay 基础库文档

本目录记录当前代码树中的 `taycpplib`、`tayclib`、`mini-cppstd` 与
`mini-cstd`。内容以 `libs/` 下的现行实现为准，不以 `.vscode/sustcore`
中的旧实现为准。

## 库的分工

| 库 | 主要职责 | 形态 |
| --- | --- | --- |
| `taycpplib` | 内核可用的 C++ 容器、算法、错误处理、格式化和辅助类型 | 含静态库与大量头文件，支持 host/freestanding |
| `tayclib` | 项目特有的 C ABI 基础类型、宏、ANSI 序列与整数转换 | C/C++ 可用，支持 host/freestanding |
| `mini-cppstd` | freestanding C++ 所需的最小 `std` 头文件表面 | 纯头文件，不是完整标准库 |
| `mini-cstd` | 最小 C 运行时头文件及字符/字符串实现 | 静态库，部分符号由更下层运行时提供 |

## 设计原则

- 内核路径不依赖异常来报告普通失败。可能失败的操作通常返回
  `tay::expected<T, tay::error_code>`。
- 动态容器通过 `tay::allocator` 分配；静态容器将容量编码进类型，避免
  堆分配。
- 若静态版与动态版只有存储不同，则公开一个 `basic_*` 算法核心，并用
  storage policy 组成具体别名。例如 `basic_fifo`、`fifo`、`static_fifo`。
- 不检查的快速接口（如多数 `operator[]`）要求调用者满足前置条件；有
  边界检查的接口通常返回 `expected`。
- `tay::panic()` 用于无法从当前 API 恢复的契约破坏或便利构造函数中的
  分配失败。内核必须提供 panic/分配钩子；host 版本由库提供默认实现。
- API 倾向于支持 `constexpr`、状态化策略对象和 freestanding 构建。

## 文档导航

- [容器](containers/README.md)
- [算法](algorithms/README.md)
- [辅助类与基础设施](helpers/README.md)
- [C/C++ 最小运行时](runtime/README.md)
- [构建、测试和头文件索引](reference/README.md)

## 快速示例

```cpp
#include <tay/expected.h>
#include <tay/static_vector.h>

tay::static_vector<int, 8> values;
auto pushed = values.push_back(42);
if (!pushed) {
    // 静态容量已满时为 error_code::OVERFLOW_ERROR。
}
```

运行 taycpplib 的主机测试和示例：

```sh
make configure config=custom
make host-test lib=taycpplib
make host-example lib=taycpplib
```

完整构建入口和当前缓存模型见 `aidoc/buildsystem/`。
