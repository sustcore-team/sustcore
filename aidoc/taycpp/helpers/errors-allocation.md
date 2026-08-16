# 错误、分配与 panic

## `expected<T, E>`

头文件：`<tay/expected.h>`。

支持拥有值、`void` 和 lvalue reference 三种 value 形态：
`expected<T, E>`、`expected<void, E>`、`expected<T&, E>`。error 必须是非
数组、非 cv 的对象类型。通过 `has_value()`/布尔转换检查，`value`、`error`、
`operator*`、`operator->` 访问内容；错误构造使用 `unexpect` 或
`unexpected<E>`。

组合接口包括 `and_then`、`transform`、`or_else`、`transform_error`。
`match` 和 `visit` 根据当前分支调用 visitor；配合 `tay::overloaded` 可把值
与错误处理写在一处：

```cpp
result.match(tay::overloaded{
    [](value& v) { use(v); },
    [](tay::error_code e) { report(e); },
});
```

`TAY_TRY(expr)` 对表达式求值一次，成功时提取 value，失败时从当前函数传播 error；
`TAY_TRYV(expr)` 只检查并传播错误；`TAY_ERR(result)` 将现有 error 包装为
`tay::unexpected`。这些宏使用 Clang/GNU statement expression 扩展，调用函数的错误类型
必须可从被传播的 error 构造。

对 `expected<void, E>`，成功分支 visitor 不接收参数。访问错误分支的 value
或成功分支的 error 会调用 `tay::panic("bad expected access")`。

## `error_code`

`<tay/err.h>` 当前定义：`NONE`、`OVERFLOW_ERROR`、`UNDERFLOW_ERROR`、
`OUT_OF_RANGE`、`NULLPTR`、`INVALID_ARGUMENT`、`OUT_OF_MEMORY` 与
`ALLOCATION_SIZE_OVERFLOW`。它是通用低层错误集合；更高层协议可使用自己
的 error enum 作为 `expected` 的第二参数。

## 分配器 `allocator`

`<tay/allocator.h>` 提供 `allocator<T>` 与 `allocator_traits`。主机环境使用
不抛异常的 new 路径；freestanding 环境调用由内核提供的 `tay::__alloc` 和
`tay::__free`。`try_allocate` 返回 `expected<pointer, error_code>`，并检查
元素数乘法溢出。

动态容器若需要无 panic 建立，应使用其 `try_create`。便利构造函数在无法
返回错误时可能 panic。

## panic 边界

`<tay/panic.h>` 声明统一终止入口。host 库提供便于测试的实现；freestanding
最终链接必须提供项目定义。panic 表示契约破坏或当前接口无法恢复的失败，
不能当作普通分支控制流。
