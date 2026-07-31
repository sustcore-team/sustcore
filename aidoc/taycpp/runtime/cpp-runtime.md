# mini-cppstd

`mini-cppstd`（元数据 id 为 `mini-cppstd`）是纯头文件的 freestanding C++
支持层。它提供当前 taycpplib/内核编译所需的标准名字，不是 libc++ 或
libstdc++ 的完整替代品。

## 当前头文件族

- 核心类型：`cstddef`、`cstdint`、`cinttypes`、`initializer_list`、`limits`；
- 类型系统：`type_traits`、`concepts`、`compare`、`ratio`；
- 对象/调用：`utility`、`tuple`、`functional`、`memory`、`new`；
- 迭代与视图：`iterator`、`span`；
- 运行时接口：`atomic`、`exception`、`source_location`；
- C wrapper：`cassert/cctype/cstdio/cstdlib/cstring/cuchar/cwchar/cstdarg`。

`memory` 当前提供 `construct_at`、`destroy_at`、`pointer_traits`、
`allocator_traits` 等基础设施；`new` 声明全局 new/delete 与 placement new。
tay 的独占智能指针位于 `<tay/unique_ptr.h>`，mini-cppstd 当前不提供完整
`std::unique_ptr/shared_ptr`。

## 环境边界

host 构建可以使用验证过的系统 C++ 标准库；freestanding 构建从此目录取得
所需标准头。代码不应因为某个头文件存在就假定完整 hosted 行为。尤其应避免
未经验证地依赖 iostream、filesystem、线程库、locale、异常运行时和标准
容器。

新增标准表面时，应先证明内核/基础库确实需要，并增加独立 headercheck 与
freestanding compile check，防止意外借用宿主标准库实现。
