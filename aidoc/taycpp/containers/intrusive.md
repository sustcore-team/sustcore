# 侵入式容器

头文件：`<tay/intrusive.h>`、`<tay/list.h>`、`<tay/tree.h>`、
`<tay/pairing_heap.h>` 与 `<tay/container_of.h>`。

侵入式容器把链接节点嵌入用户对象，不为元素分配内存，也不拥有元素生命期。
它们适合调度队列、等待队列、缓存索引和内核对象注册表。

## 侵入式链表

对象包含 list hook，并通过 accessor/member hook 把对象映射到节点。
`intrusive_list` 提供双向迭代、头尾插入、指定位置插入、擦除和 splice
一类操作。一个 hook 同一时刻只能属于一个链表；若对象需要同时进入多个
链表，必须嵌入多个独立 hook/tag。

容器析构不会 `delete` 元素。移动或销毁已链接对象会留下悬空链接，因此应先
erase/unlink。插入/擦除不影响其他节点地址和迭代器。

## 侵入式树

`intrusive_tree` 使用嵌入式 hook 表达非拥有式多叉层级关系。它不保存 root，
支持头尾链接、指定 sibling 前链接、摘链、reparent、直接子节点遍历和深度优先
前序/后序遍历。`intrusive_tree_hook` 缓存尾子节点并提供 O(1) 尾插；
`compact_intrusive_tree_hook` 节省一个指针，但尾插需要 O(k) 扫描直接子节点。

节点析构前必须先摘链，并确保其直接子节点已经迁移或清空。移动已链接节点会让
父子和兄弟指针失效。

## 侵入式 pairing heap

`intrusive_pairing_heap<T, Locate, Compare>` 是无分配的 min-priority queue。
对象嵌入 `intrusive_pairing_heap_hook<T>`，`Locate` 负责定位 hook，`Compare`
可保存状态并定义严格弱序。`intrusive_priority_queue` 是同一类型的别名。

主要接口如下：

- `empty()`、`size()` 与 `top()` 查询队列；空队列的 `top()` 返回空指针；
- `push(node)` 以 O(1) 插入，重复链接或残留 hook 会 panic；
- `pop_min()` 摊销 O(log n) 移除比较序最前的节点；空队列调用会 panic；
- `remove(node)` 与 `remove(hook)` 摊销 O(log n) 删除已知节点；
- `clear()` 以 O(n) 摘除全部节点，但不析构节点。

heap 调整只改写 hook，不移动对象，因此对象地址保持稳定。节点从 `push()` 到
`pop_min()`、`remove()` 或 `clear()` 期间必须保持原地址且不得析构；参与比较的
key 也不得以破坏堆序的方式修改。heap 析构会调用 `clear()`，所以仍链接的节点
必须至少存活到 heap 析构完成。摘除后 hook 会恢复为未链接状态，可再次插入。

容器不提供内部同步；跨执行上下文共享时，调用方必须用覆盖全部 heap 与 hook
修改的同一同步协议保护操作。

## 适用边界

- 优点：零节点分配、稳定对象地址、一个对象可通过不同 hook 进入不同索引。
- 代价：生命周期由调用者负责，误用会造成悬空指针；hook 配置比拥有型容器
  更显式。
