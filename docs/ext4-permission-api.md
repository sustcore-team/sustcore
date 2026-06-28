# ext4 权限接口设计

## 1. 数据结构

### AttrSet — inode 属性集

```cpp
// include/sustcore/attr.h
struct AttrSet {
    uint32_t mode;    // 文件类型 + 权限位 (S_IFREG | 0755)
    uint32_t uid;     // owner uid（ext4 磁盘 16-bit，扩展至 32-bit）
    uint32_t gid;     // owner gid
    uint64_t size;    // 文件大小
    uint64_t inode;   // inode 编号
    uint32_t nlink;   // 硬链接数
    uint64_t atime;   // access time (Unix 秒)
    uint64_t mtime;   // modify time
    uint64_t ctime;   // change time
    uint32_t blksize; // 块大小，默认 512
    uint64_t blocks;  // 块数
};
```

### AttrMask — setattr 修改掩码

```cpp
enum class AttrMask : uint32_t {
    MODE  = 1 << 0,
    UID   = 1 << 1,
    GID   = 1 << 2,
    SIZE  = 1 << 3,
    ATIME = 1 << 4,
    MTIME = 1 << 5,
    CTIME = 1 << 6,
    // 常用组合
    OWNER = UID | GID,    // fchownat 使用
    TIMES = ATIME | MTIME,
};
constexpr AttrMask operator|(AttrMask a, AttrMask b);
constexpr bool    operator&(AttrMask a, AttrMask b);
```

> **设计要点**: `AttrSet` 独立于现有 `NodeMeta`（用户态 ABI），不改动 `include/sustcore/files.h`。

---

## 2. VFS 接口

### IINode 新增两个纯虚方法

```cpp
// kernel/vfs/ops.h — IINode 类新增
class IINode : public RTTIBase<IINode, INodeType> {
public:
    // 已有方法保持不变...

    [[nodiscard]]
    virtual Result<void> getattr(AttrSet &out) const = 0;

    [[nodiscard]]
    virtual Result<void> setattr(AttrMask mask, const AttrSet &attrs) = 0;
};
```

### 默认实现（基类 fallback）

`IFile`、`IDirectory`、`ISymlink` 中各加一个默认实现，返回 `ErrCode::NOT_SUPPORTED`。已有文件系统（tarfs、tmpfs、devfs）编译通过即可，无需额外改动。

---

## 3. 文件系统实现

### 3.1 ext4

**数据来源**: ext4 磁盘 inode raw data（byte-offset 读写，不引入 struct）

| 属性 | ext4 offset | 大小 | 说明 |
|------|-------------|------|------|
| mode  | 0 | u16 | 文件类型 + 权限 |
| uid   | 2 | u16 | → AttrSet.uid（高位填 0） |
| gid   | 24 (0x18) | u16 | → AttrSet.gid |
| size  | 4 + 108 (0x6C) | u32 + u32 | 仅文件/符号链接有 high 32-bit |
| atime | 8 | u32 | Unix epoch |
| ctime | 12 | u32 | |
| mtime | 16 | u32 | |
| nlink | 26 (0x1A) | u16 | |

**读写方式**: 与现有 `inode_mode()` 一致——`read_inode_raw()` → byte-offset 提取 → 填充 AttrSet；setattr 则是读取 → 修改 → `write_inode_raw()` 写回。

**Ext4File / Ext4Directory / Ext4Symlink** 三者的 `getattr` / `setattr` 都委托给 `Ext4Superblock` 的 `fill_attr()` / `apply_attr()` 方法。

### 3.2 procfs

- **getattr**: 所有节点返回固定默认值（uid=0, gid=0, mode 按节点类型给 0444/0555/0755/0777, nlink=1）
- **setattr**: 默认返回 `ErrCode::NOT_SUPPORTED`；特殊节点可 override（例如将来 /proc/sys 下的可写属性）

### 3.3 其他伪文件系统（tarfs, tmpfs, devfs）

继承 `IFile` / `IDirectory` 的默认 `NOT_SUPPORTED` 实现，行为不变。

---

## 4. 系统调用: fchownat

### syscall 注册

```cpp
// include/sustcore/syscall.h
#define SYS_VFS_FCHOWNAT (SYSCALL_BASE + 0x3E)

// kernel/syscall/syscall.cpp — dispatch_sync()
case SYS_VFS_FCHOWNAT: {
    CapIdx  dirfd  = capidx;
    uint32_t uid   = arg0;
    uint32_t gid   = arg1;
    uint32_t flags = arg2;
    UString  path  = get_user_string(arg3);
    // → vfs_fchownat(dirfd, path, uid, gid, flags)
}
```

### 内核实现 `vfs_fchownat`

```
vfs_fchownat(dirfd, relpath, uid, gid, flags)
  1. dirfd == AT_FDCWD → 使用当前工作目录
  2. VFS 路径解析，找到目标 vnode
     - flags & AT_SYMLINK_NOFOLLOW → 不跟随符号链接
     - flags & AT_EMPTY_PATH  → path 为空时用 dirfd 自身
  3. 构造 AttrSet{uid, gid} + AttrMask::OWNER
  4. 调用 vnode->inode()->setattr(AttrMask::OWNER, attrs)
  5. 返回结果
```

### libc 用户态接口

```c
// RISC-V 汇编 stub: libs/linuxss-libc/arch/riscv64/asm_syscall.S
ENTRY(fchownat)
    li a7, SYS_VFS_FCHOWNAT
    ecall
    ret
END(fchownat)

// C 封装
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);
```

---

## 5. VFS stat 路径重构

现有 `VFS::_stat_from_vinode()` 硬编码了 `type/size/inode/links/devno`。重构为：

```
_stat_from_vinode(vnode, NodeMeta &out)
  1. 调用 vnode.inode()->getattr(attr)
  2. 若成功:
     out.type  = attr.mode & S_IFMT → EntryType
     out.size  = attr.size
     out.inode = attr.inode
     out.links = attr.nlink
     out.devno = /* 从 mount table 查 */
  3. 若 getattr 返回 NOT_SUPPORTED:
     回退到现有硬编码逻辑（兼容未实现 getattr 的文件系统）
```

---

## 6. 测试

在 `module/test_ext4_permission/` 下实现用户态测试模块，覆盖：

| 用例 | 操作 | 验证 |
|------|------|------|
| getattr file | 打开文件 → getattr | mode 包含 S_IFREG |
| getattr dir | 打开目录 → getattr | mode 包含 S_IFDIR |
| setattr uid | setattr(uid=1000) → getattr | uid == 1000 |
| setattr gid | setattr(gid=500) → getattr | gid == 500 |
| fchownat basic | fchownat(uid=2000, gid=2000) → getattr | uid=2000, gid=2000 |
| fchownat nofollow | symlink → fchownat(AT_SYMLINK_NOFOLLOW) | link 自身 uid 变，target 不变 |
| fchownat enoent | fchownat 不存在的路径 | 返回 ENOENT |

---

## 7. 兼容性保证

| 不改动的 | 原因 |
|----------|------|
| `NodeMeta` (files.h:70) | 用户态 ABI，已有 stat/lstat/fstat 依赖 |
| `IMetadata` (ops.h:71) | 空基类，无扩展价值（属性通过 IINode 直接传递） |
| `IFsDriver` / `ISuperblock` | 文件系统注册层，与属性读写无关 |
| 已有 syscall 编号 (0x24-0x3D) | 保持向后兼容 |
| ext4 现有 byte-offset 读写模式 | 保持一致代码风格 |
