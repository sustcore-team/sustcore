# 内存能力规范

本文档描述了Sustcore操作系统中内存能力（memory capability）的设计规范。内存能力是一种用于管理和保护内存资源的机制，确保进程只能访问其被授权的内存区域。
内存能力通过能力对象（capability object）来表示，包含了对特定内存区域的访问权限和属性。

## 规范

### 1. 定义

#### 1. 内存能力

内存能力一共包含以下权限:

```c
typedef struct {
    bool priv_getaddr; // 允许获取内存区域的物理地址和大小
    bool priv_map;     // 允许将内存区域映射到进程的虚拟地址空间
                       // 映射时需要指定访问权限（读/写/执行）
                       // 指定权限时需遵循内存能力本身的权限限制
                       // 例如: 如果内存能力不允许写入, 则映射时也不能指定写权限
                       // 最基础的映射权限是允许读取
    bool priv_unmap;   // 允许将内存区域从进程的虚拟地址空间中解除映射
    bool priv_read;    // 允许读取内存区域
    bool priv_write;   // 允许写入内存区域
    bool priv_execute; // 允许执行内存区域中的代码
    bool priv_share;   // 允许将该能力分享给其他进程
                       // 要求内存能力必须要是一个共享内存能力
                       // 即MemCapData::shared==true
} MemCapPriv;
```

内存能力一共包含以下:

```c
typedef struct {
    void *paddr;    // 内存区域的起始地址
    size_t size;    // 内存区域的大小
    bool root;      // 指示该内存能力是否为根能力
                    // 根能力表示该能力是直接分配的内存区域
                    // 而非从其他能力派生而来
    bool shared;    // 指示该内存区域是否为共享内存
    bool mmio;      // 指示该内存区域是否为内存映射I/O区域
    bool naked;     // 指示该内存区域是否为裸内存
                    // 即该块内存区域在物理上是连续的
                    // 诸如与DMA交流时需要使用裸内存
} MemCapData;
```

#### 2. 映射标志

```c
typedef struct {
    bool read    : 1;      // 允许读取
    bool write   : 1;      // 允许写入
    bool execute : 1;      // 允许执行
} MemMapFlags;
```

### 2. 对内存能力的操作

#### 2.1 获取内存区域地址和大小

前提: `MemCapPriv::priv_getaddr`

允许进程通过内存能力获取内存区域的物理地址和大小。

```c
void *memcap_get_addr(PCB *pcb, CapPtr cap);
```

```c
size_t memcap_get_size(PCB *pcb, CapPtr cap);
```

#### 2.2 映射内存区域

前提: 
 - `MemCapPriv::priv_map`
 - `MemMapFlags::read <= MemCapPriv::priv_read`
 - `MemMapFlags::write <= MemCapPriv::priv_write`
 - `MemMapFlags::execute <= MemCapPriv::priv_execute`

允许进程将内存区域映射到其虚拟地址空间。映射时需要指定访问权限（读/写/执行），并且必须遵循内存能力本身的权限限制。

```c
void *memcap_map(PCB *pcb, CapPtr cap, MemMapFlags flags);
```

#### 2.3 解除映射内存区域

前提: `MemCapPriv::priv_unmap`

允许进程将内存区域从其虚拟地址空间中解除映射。

```c
void memcap_unmap(PCB *pcb, CapPtr cap, void *vaddr);
```

#### 2.4 创建共享内存能力

允许进程创建一个新的共享内存能力。

```c
CapPtr memcap_create_shm(PCB *pcb, size_t size, MemMapFlags flags);
```

创建出的能力权限为:
 - `MemCapPriv::priv_getaddr = false`
 - `MemCapPriv::priv_map = true`
 - `MemCapPriv::priv_unmap = true`
 - `MemCapPriv::priv_read = flags.read`
 - `MemCapPriv::priv_write = flags.write`
 - `MemCapPriv::priv_execute = flags.execute`
 - `MemCapPriv::priv_share = true`
属性为
 - `MemCapData::paddr` 为分配的物理内存地址
 - `MemCapData::size = size`
 - `MemCapData::shared = true`
 - `MemCapData::mmio = false`
 - `MemCapData::naked = false`
 - `MemCapData::root = true`

#### 2.5 创建MMIO内存能力

允许进程创建一个新的内存映射I/O（MMIO）能力。
需要持有该MMIO对应的设备能力.
设备能力里面包含了MMIO的物理地址和大小等信息。

前提:
 - `PCBCapPriv::priv_alloc_mmio`
 - `DevCapPriv::priv_mmio`

```c
CapPtr memcap_create_mmio(PCB *pcb, CapPtr device_cap);
```

创建出的能力权限为:
 - `MemCapPriv::priv_getaddr = true`
 - `MemCapPriv::priv_map = true`
 - `MemCapPriv::priv_unmap = true`
 - `MemCapPriv::priv_read = DevCapPriv::read`
 - `MemCapPriv::priv_write = DevCapPriv::write`
 - `MemCapPriv::priv_execute = false`
 - `MemCapPriv::priv_share = false`
属性为
 - `MemCapData::paddr = DevCapPrive::mmio_base_addr`
 - `MemCapData::size = DevCapPrive::mmio_size`
 - `MemCapData::shared = false`
 - `MemCapData::mmio = true`
 - `MemCapData::naked = false`
 - `MemCapData::root = false`

并且该能力会被视作是`device_cap`派生出的能力.

#### 2.6 创建裸内存能力

允许进程创建一个新的裸内存能力。

前提: `PCBCapPriv::priv_alloc_naked`

```c
CapPtr memcap_create_naked(PCB *pcb, size_t size, MemMapFlags flags);
```

创建出的能力权限为:
 - `MemCapPriv::priv_getaddr = true`
 - `MemCapPriv::priv_map = true`
 - `MemCapPriv::priv_unmap = true`
 - `MemCapPriv::priv_read = flags.read`
 - `MemCapPriv::priv_write = flags.write`
 - `MemCapPriv::priv_execute = flags.execute`
 - `MemCapPriv::priv_share = false`
属性为
 - `MemCapData::paddr` 为分配的物理内存地址
 - `MemCapData::size = size`
 - `MemCapData::shared = false`
 - `MemCapData::mmio = false`
 - `MemCapData::naked = true`
 - `MemCapData::root = true`

#### 2.6 转发内存能力

允许进程将其内存能力转发给其他进程。
在这个过程中，接收进程将获得一个新的内存能力对象，权限和属性与原始能力相同。
原进程将不再拥有该能力。

前提: `MemCapPriv::priv_share`

```c
CapPtr memcap_forward(PCB *src_pcb, CapPtr src_cap, PCB *dst_pcb);
```

#### 2.7 派生内存能力

允许进程从其现有的内存能力中派生出一个新的内存能力对象。
派生出的能力权限可以是原始能力权限的子集，但不能包含原始能力中没有的权限。

前提:
 - `new_priv.priv_getaddr <= src_priv.priv_getaddr`
 - `new_priv.priv_map <= src_priv.priv_map`
 - `new_priv.priv_unmap <= src_priv.priv_unmap`
 - `new_priv.priv_read <= src_priv.priv_read`
 - `new_priv.priv_write <= src_priv.priv_write`
 - `new_priv.priv_execute <= src_priv.priv_execute`
 - `new_priv.priv_share <= src_priv.priv_share`

```c
CapPtr memcap_derive(PCB *pcb, CapPtr src_cap, MemCapPriv new_priv);
```

创建出的能力权限为`new_priv`，属性与`src_cap`相同。
该能力将会是`src_cap`派生出的能力.

且`MemCapData::root = false`

#### 2.8 注销内存能力

允许进程注销其内存能力，释放相关的内存资源。

```c
void memcap_revoke(PCB *pcb, CapPtr cap);
```

当`MemCapData::root`时, 释放该内存区域所占用的物理内存.
同时注销所有从该能力派生出的能力.