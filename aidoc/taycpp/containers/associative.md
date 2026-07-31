# 关联容器

## `hash_map`

头文件：`<tay/map.h>`。

```cpp
template<class Key, class T,
         class Hash = std::hash<Key>,
         class KeyEqual = std::equal_to<Key>,
         class Allocator = tay::allocator<std::pair<const Key, T>>>
class hash_map;
```

这是动态分配、唯一键、链式桶哈希表。模板参数顺序遵循常见容器习惯：键、
值、哈希、等价比较、分配器。支持状态化 `Hash` 和 `KeyEqual`，并可由
`hash_function()`、`key_eq()` 取回副本。主要接口包括迭代、`find`、
`contains`、`at`、`operator[]`、`insert/emplace/try_emplace`、`erase`、
`reserve`、`rehash` 和负载因子控制。

节点地址通常不会因 rehash 改变，但桶和迭代次序会改变；擦除使对应节点的
引用与迭代器失效。普通失败使用 `expected`。

## hash set storage family

头文件：`<tay/set.h>`。

```cpp
basic_hash_set<Key, Storage, Hash, KeyEqual>
hash_set<Key, Hash, KeyEqual, Allocator>
static_hash_set<Key, N, Hash, KeyEqual>
```

`basic_hash_set` 实现哈希、查找、负载控制和迭代；Storage 提供桶与节点。
动态版按需分配并支持 rehash，静态版最多保存 `N` 个键且不分配。静态版
容量耗尽返回 `OVERFLOW_ERROR`。`Hash` 与 `KeyEqual` 通过 composition
保存，空策略通常不增加对象大小，状态化策略仍保持实例状态。

## flat containers

头文件：`<tay/flat.h>`。

`basic_flat_set<Key, Sequence, Compare>` 和
`basic_flat_map<Key, T, Sequence, Compare>` 把排序/二分查找算法与底层连续
序列分开。公开动态别名 `flat_set`、`flat_map` 使用 `array_list`。

查找为对数复杂度；在连续序列中插入/擦除仍为线性复杂度。它们适合元素量
不大、查找较多、希望减少节点分配并改善缓存局部性的表。插入可能让所有
迭代器失效，map 迭代器暴露类似 `pair<const Key, T>` 的代理引用。

底层 Sequence 只要提供相应连续序列 API，也可组合成固定容量版本：

```cpp
using small_set = tay::basic_flat_set<
    int, tay::static_vector<int, 16>>;
```

所有容器都要求比较关系满足严格弱序；修改 key 破坏排序/哈希不变量。
