# PFA (Page Frame Allocator) 页框分配器

PFA提供了一族函数, 用于分配并回收物理内存页框.
该功能的经典实现是伙伴系统 (Buddy System).

mem.puml 中的 PFATrait 接口定义了 PFA 的基本操作.
以下部分将详细描述这些操作的语义和用法.

## 1. PFA

### 1.1 void pfa_init(size_t total_frames)

初始化 PFA 子系统.
参数 total_frames 指定了系统中可用的物理页框总数.

### 1.2 void pfa_post_init(void)

在系统初始化的后期阶段调用该函数.
这里, 系统初始化的后期阶段指将内核地址迁移到高端内存之后的阶段.

由于PFA在系统启动的早期阶段就已经被初始化, 其内部存储的指针有可能仍然指向低端内存区域.
因此, 在内核地址迁移到高端内存之后, 需要调用该函数来更新PFA内部的指针, 以确保其指向正确的内存区域.

### 1.3 void *pfa_alloc_frame(void)

分配一个物理页框, 并返回该页框的起始地址.
如果没有可用的页框, 则返回 nullptr.

### 1.4 void *pfa_alloc_frames(size_t count)

分配 count 个连续的物理页框, 并返回第一个页框的起始地址.
如果没有足够的连续页框可用, 则返回 nullptr.

### 1.5 void pfa_free_frame(void *frame)

回收由 pfa_alloc_frame 分配的单个物理页框.

### 1.6 void pfa_free_frames(void *frame, size_t count)

回收由 pfa_alloc_frames 分配的连续物理页框.

## 2. Buddy System 伙伴系统

PFA 的经典实现是伙伴系统 (Buddy System).
伙伴系统通过将物理内存划分为大小为 2 的幂次的块来管理内存.
当分配或回收内存时, 伙伴系统会根据请求的大小找到合适的块, 并在必要时进行分裂或合并操作.

### 2.1 void buddy_init(size_t total_frames)
### 2.2 void buddy_post_init(void)
### 2.3 void *buddy_alloc_frame(void)
### 2.4 void *buddy_alloc_frames(size_t count)
### 2.5 void *buddy_alloc_frame_in_order(int order)
### 2.6 void buddy_free_frame(void *frame)
### 2.7 void buddy_free_frames(void *frame, size_t count)
### 2.8 void buddy_free_frame_in_order(void *frame, int order)
### 2.9 int buddy_pages2order(size_t pages)