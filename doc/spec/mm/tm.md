# TM (Task Memory)

与进程相关的内存管理模块。

## 1. RPMA (Referable Physical Memory Allocator) 可引用物理内存分配器

RPMA 是一种物理内存分配器，支持对分配的物理内存块进行引用计数管理。它允许多个内存管理单元共享同一块物理内存，并通过引用计数来跟踪内存的使用情况。当引用计数降为零时，内存块将被释放回系统。

一般来说, 只有 Task Memory 与 Memory Capability 会使用 RPMA 来分配物理内存。

### 1.1 void rpma_init(void)

初始化 RPMA 子系统。该函数在系统启动时调用一次，设置必要的数据结构和状态，以便后续的内存分配和管理操作能够正常进行。

### 1.2 PMA *rpma_alloc_frames(size_t count)

分配指定数量的物理内存页面。RPMA并不保证分配的内存帧是连续的。因此其会返回一个链表, 每个链表代表一块物理内存区域。区域中的页面总数为 `count`。
同时, 每个页面的引用计数会被初始化为1。

### 1.3 void rpma_free_frames(PMA *pma)

释放由 `rpma_alloc_frames` 分配的物理内存页面。该函数会递减每个页面的引用计数，当引用计数降为零时，页面将被释放回系统。

### 1.4 void rpma_ref_frames(PMA *pma, qword flags)

增加指定物理内存页面的引用计数。该函数允许多个内存管理单元共享同一块物理内存，通过增加引用计数来表示对该内存的使用。

特别地, flags 将会说明该页面以何种方式引用, 并将该标志存储在页面的附加信息中。

### 1.5 void rpma_deref_frame(void *paddr, size_t size)

减少指定物理内存页面的引用计数。该函数会递减每个页面的引用计数，当引用计数降为零时，页面将被释放回系统。

### 1.6 AdditionalFrameInfo *rpma_get_additional_info(void *paddr)

获取指定物理内存页面的附加信息。该函数返回一个指向 `AdditionalFrameInfo` 结构的指针，该结构包含了与页面相关的额外信息，如引用标志等。