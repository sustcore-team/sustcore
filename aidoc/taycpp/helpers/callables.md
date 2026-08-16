# 可调用对象与 utility

## `function_ref<R(Args...)>`

头文件：`<tay/functional.h>`。

非拥有、固定两个指针量级的 callable view，适合临时回调参数。它不分配，
也不延长 callable 生命周期；不能把指向局部 lambda 的 function_ref 保存到
更长生命周期对象。另有 `noexcept` signature 特化，用类型系统要求目标调用
不抛异常。

## `inplace_function<R(Args...), N>`

拥有 callable，并把对象存进 `N` 字节内联缓冲区，不使用堆。它使用内部
vtable 完成调用、复制、移动和销毁；目标必须满足尺寸、对齐和构造约束。
空对象可用布尔转换检查并可 `reset()`。也提供 `noexcept` signature 特化。

它适合中断回调、驱动完成函数等需要拥有状态但禁止动态分配的场景。容量 `N`
应按最大目标对象确定，过大的捕获 lambda 在编译期被拒绝。

## `composition<Tag, T>`

头文件：`<tay/utility.h>`。

composition 用 tag 区分同一宿主中的多个策略，并对类类型使用空基类优化；
非类类型作为普通成员保存。`tay::get<Tag>(this)` 取回策略。当前 hash set、
logger 等用它保存状态化 Hash/KeyEqual/Output，同时让空策略不占额外空间。

## `projected_compare<Compare, Projection>`

头文件：`<tay/utility.h>`。

`projected_compare` 保存 Compare 与 Projection 对象，调用时先分别投影左右操作数，
再比较投影结果；两种策略都可以携带状态。成员指针可直接作为 Projection 类型，
并在构造时传入具体成员：

```cpp
using deadline_less = tay::projected_compare<std::ranges::less, u64_t TimerNode::*>;
deadline_less compare(std::ranges::less{}, &TimerNode::deadline);
```

Projection 也可以是普通函数对象；构造时应同时传入 Compare 与 Projection，避免
默认构造的空成员指针成为无效投影。

## `overloaded<Ts...>`

把多个 callable 的 `operator()` 合并为一个重载集，适合 `expected::match`：

```cpp
auto visitor = tay::overloaded{
    [](int value) { return value; },
    [](error err) { return fallback(err); },
};
```

deduction guide 会 decay callable 类型，因此可直接传 lambda。重载必须对所有
可能分支可调用，并应返回兼容的结果类型。
