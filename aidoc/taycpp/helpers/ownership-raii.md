# 所有权与 RAII

可以分别包含下述类型的头文件，也可以包含汇总头 `<tay/raii.h>`。汇总头还
包含 `<tay/lock.h>` 中的锁所有权类型；完整说明见
[锁与受保护对象](locking.md)。

## `guard<Fn>`

头文件：`<tay/guard.h>`。

`guard` 保存一个清理函数并在离开作用域时调用。它不可复制、不可移动，避免
一次清理责任被多个对象持有。`release()` 解除清理，`active()` 查询状态。

```cpp
lock(resource);
tay::guard unlock_on_exit{[&] { unlock(resource); }};
if (commit()) {
    unlock_on_exit.release();
}
```

析构函数为 `noexcept`，因此清理函数不得抛出异常；freestanding 内核本来也
不应依赖异常传播。guard 的生命期应覆盖其 lambda 捕获对象的生命期。

## `owner<T*>`

头文件：`<tay/owner.h>`。

`owner` 是 GSL 风格的所有权标注，不会自动 delete。它适合在 API 中表达
“该裸指针代表所有权”，但当前类型本身可复制，仍需调用者保持唯一释放约定。
若需要真正的独占 RAII，应使用 `unique_ptr`。

## `unique_ptr<T>` 与 `unique_ptr<T[]>`

头文件：`<tay/unique_ptr.h>`。

两者均不可复制、可以移动。标量版使用 `delete`，提供 `operator*` 和
`operator->`；数组版使用 `delete[]`，提供 `operator[]`。共同 API 包括：

- 默认/null/裸指针构造；
- 从右值 `owner<U*>` 接管所有权；
- `get()`、布尔转换；
- `release()`、`release_owner()`、`reset()`、`swap()`；
- `make_unique<T>(args...)` 与 `make_unique<T[]>(count)`。

```cpp
auto object = tay::make_unique<device>(args...);
auto bytes = tay::make_unique<std::byte[]>(4096);
```

本实现有意采用用户请求的一参数模板，不提供自定义 deleter。内存必须由与
`delete`/`delete[]` 匹配的 `new`/`new[]` 获得；MMIO、页分配器对象或需要
特殊关闭流程的资源应使用 `guard` 或专用 RAII 类型。

解引用空指针、数组越界与错误的 new/delete 配对均为调用者错误。把派生类
`unique_ptr` 转为基类所有权时，若未来通过基类删除，基类应有虚析构函数。

## `refcount`

`<tay/refcount.h>` 提供项目的引用计数基础设施。它用于共享生命周期，而非
替代所有裸观察指针。循环引用不能自动打破；内核对象图应明确强/弱关系。
