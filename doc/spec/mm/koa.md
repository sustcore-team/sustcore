# KOA (Kernel Object Allocator) 内核对象分配器

KOA 提供了一族函数, 用于分配并回收内核对象.
该功能最简单的实现就是使用kmalloc和kfree来分配和回收内核对象.
然而, 更加高效的实现参见slab, slub等内核对象分配器.

## 1. KOA

KOCache 是内核对象缓存的抽象表示. 其并没有明确定义, 随着不同的KOA实现而变化.

### 1.1 KOCache *koa_create_cache(const char *name, size_t ko_size, size_t align, qword flags)

创建一个内核对象缓存.
参数 name 指定了缓存的名称.
参数 ko_size 指定了内核对象的大小.
参数 align 指定了内核对象的对齐要求.
参数 flags 指定了缓存的属性标志.
返回值为创建的内核对象缓存的指针.

### 1.2 void koa_destroy_cache(KOCache *cache)

销毁一个内核对象缓存.

### 1.3 void *koa_alloc(KOCache *cache, qword flags)

从指定的内核对象缓存中分配一个内核对象.
参数 cache 指定了内核对象缓存.
参数 flags 指定了分配的属性标志.

### 1.4 void koa_free(KOCache *cache, void *ko)

回收一个内核对象.
参数 cache 指定了内核对象缓存.
参数 ko 指定了要回收的内核对象.

## 2. Trival KOA

Trival KOA 是 KOA 的一种简单实现.
其直接使用 kmalloc 和 kfree 来分配和回收内核对象.

### 2.1 TrivalKOCache 结构体

该结构体实际上没有任何缓存, 只是为了符合 KOA Trait 的接口要求,
并保留了例如 name, ko_size, align, flags 等字段

其定义为

```c
typedef struct {
    const char *name;
    size_t ko_size;
    size_t align;
    qword flags;
} TrivalKOCache;
```

### 2.1 TrivalKOCache *trival_koa_create_cache(const char *name, size_t ko_size, size_t align, qword flags)

创建一个 Trival KOA 内核对象缓存.
这个函数实际上并没有创建任何缓存结构, 只是为了符合 KOA Trait 的接口要求.

### 2.2 void trival_koa_destroy_cache(TrivalKOCache *cache)

销毁一个 Trival KOA 内核对象缓存.

### 2.3 void *trival_koa_alloc(TrivalKOCache *cache, qword flags)

从指定的 Trival KOA 内核对象缓存中分配一个内核对象.
该函数实际上调用 kmalloc 来分配内核对象.

### 2.4 void trival_koa_free(TrivalKOCache *cache, void *ko)

回收一个 Trival KOA 内核对象.
该函数实际上调用 kfree 来回收内核对象.

## 3. Slub KOA

Slub KOA 是 KOA 的一种高效实现.