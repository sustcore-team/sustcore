# 侵入式容器

头文件：`<tay/intrusive.h>`、`<tay/list.h>`、`<tay/tree.h>` 与
`<tay/container_of.h>`。

侵入式容器把链接节点嵌入用户对象，不为元素分配内存，也不拥有元素生命期。
它们适合调度队列、等待队列、缓存索引和内核对象注册表。

## intrusive list

对象包含 list hook，并通过 accessor/member hook 把对象映射到节点。
`intrusive_list` 提供双向迭代、头尾插入、指定位置插入、擦除和 splice
一类操作。一个 hook 同一时刻只能属于一个链表；若对象需要同时进入多个
链表，必须嵌入多个独立 hook/tag。

容器析构不会 `delete` 元素。移动或销毁已链接对象会留下悬空链接，因此应先
erase/unlink。插入/擦除不影响其他节点地址和迭代器。

## intrusive tree

`intrusive_tree` 使用嵌入式树节点以及 KeyOf/Compare 策略组织二叉搜索树。
它不分配节点，支持插入、查找、边界查询、迭代与擦除。比较策略可以有状态。
对象 key 在链接期间必须保持排序意义不变。

当前 intrusive tree 是非平衡搜索树：平均操作可接近 O(log n)，退化输入下
可达到 O(n)。需要最坏情况保证时应在上层控制插入模式，或后续引入平衡树。

## 适用边界

- 优点：零节点分配、稳定对象地址、一个对象可通过不同 hook 进入不同索引。
- 代价：生命周期由调用者负责，误用会造成悬空指针；hook 配置比拥有型容器
  更显式。
