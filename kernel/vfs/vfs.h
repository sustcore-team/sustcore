/**
 * @file vfs.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual File System
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <device/block.h>
#include <kio.h>
#include <sus/list.h>
#include <sus/map.h>
#include <sus/nonnull.h>
#include <sus/owner.h>
#include <sus/path.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <string>

class VFsDriver;
class VSuperblock;
class VINode;
class VFile;
class DEntry;

class VFsDriver : public util::refc<VFsDriver> {
public:
    constexpr void on_death() {}
    constexpr bool closable() const {
        return !alive();
    }

private:
    util::owner<IFsDriver *> _fsd;

public:
    constexpr VFsDriver(util::owner<IFsDriver *> fsd) : _fsd(fsd) {}
    constexpr virtual ~VFsDriver() {
        delete _fsd;
    }
    constexpr IFsDriver *fsd() const {
        return _fsd.get();
    }
};

class VSuperblock : public util::refc<VSuperblock> {
public:
    constexpr void on_death() {}
    constexpr bool closable() const {
        return !alive();
    }

private:
    util::owner<ISuperblock *> _sb;
    util::refc_ptr<VFsDriver> _fsd;
    util::LinkedMap<inode_t, util::owner<VINode *>> inode_cache;

public:
    VSuperblock(util::owner<ISuperblock *> sb, VFsDriver &fsd)
        : _sb(sb), _fsd(&fsd) {}
    virtual ~VSuperblock() {
        assert(inode_cache.empty());
        delete _sb;
    }
    constexpr ISuperblock *sb() const {
        return _sb.get();
    }
    constexpr VFsDriver &vfsd() const {
        return *_fsd;
    }
    // 是否仍有该超级块上的 inode 处于活动状态
    constexpr bool busy() const {
        return !inode_cache.empty();
    }
    // 更新inode_cache, 使其包含指定inode号的inode
    Result<VINode *> update_inode(inode_t inode_id);
    // 整理inode_cache
    Result<void> tidy_up();
};

class VINode : public util::refc<VINode> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::VFILE;
    void on_death() {}
    constexpr bool closable() const {
        return !alive();
    }

private:
    util::owner<IINode *> _inode;
    util::refc_ptr<VFsDriver> _fsd;
    util::refc_ptr<VSuperblock> _vsb;

public:
    constexpr IINode *inode() const {
        return _inode.get();
    }
    constexpr VFsDriver &vfsd() const {
        return *_fsd;
    }
    constexpr VSuperblock &superblock() const {
        return *_vsb;
    }

    constexpr VINode(util::owner<IINode *> inode, VFsDriver &fsd,
                     VSuperblock &vsb)
        : _inode(inode), _fsd(&fsd), _vsb(&vsb) {}
    constexpr virtual ~VINode() {
        delete _inode;
    }
};

class VFile : public cap::_PayloadHelper<PayloadType::VFILE> {
private:
    util::nonnull<VINode *> _vind;
    bool _discarded = false;

public:
    explicit VFile(util::nonnull<VINode *> vind) : _vind(vind) {
        _vind->keep();
    }

    ~VFile() override {
        if (!_discarded) {
            loggers::VFS::WARN("VFile destructed without being discarded");
        }

        _vind->release();
    }

    void destruct() override {
        if (_discarded) {
            loggers::VFS::WARN("VFile destructed multiple times");
            return;
        }
        _discarded = true;
    }

    [[nodiscard]]
    util::nonnull<VINode *> vind() const {
        return _vind;
    }

    [[nodiscard]]
    bool discarded() const {
        return _discarded;
    }
};

class DEntry : public util::refc<DEntry> {
public:
    constexpr void on_death() {}

    [[nodiscard]]
    constexpr bool closable() const {
        return !alive();
    }

private:
    util::Path _path;
    // automatically call the keep() and release() of VINode and DEntry
    util::refc_ptr<VINode> _vind;
    util::refc_ptr<DEntry> _parent;

public:
    constexpr DEntry(const util::Path &path, util::nonnull<VINode *> vind,
                     DEntry * parent)
        : _path(path), _vind(vind), _parent(parent) {}
    constexpr virtual ~DEntry() {}
    constexpr const util::Path &path() const {
        return _path;
    }
    constexpr util::nonnull<VINode *> vind() const {
        return util::nnullforce(_vind.get());
    }
    constexpr DEntry * parent() const {
        return _parent.get();
    }
};

enum class MountFlags { NONE = 0 };

class VFS {
private:
    util::LinkedMap<std::string, util::owner<VFsDriver *>> fs_table;
    util::LinkedMap<util::Path, util::owner<VSuperblock *>> mount_table;
    util::LinkedMap<util::Path, util::owner<DEntry *>> dentry_cache;
    util::ArrayList<VFile *> open_files;

public:
    VFS() = default;
    ~VFS();

    VFS(const VFS &)            = delete;
    VFS &operator=(const VFS &) = delete;
    VFS(VFS &&)                 = delete;
    VFS &operator=(VFS &&)      = delete;

    // 注册文件系统
    Result<void> register_fs(util::owner<IFsDriver *> &&driver);
    Result<void> unregister_fs(const char *fs_name);
    // 挂载文件系统
    Result<void> mount(const char *fs_name, IBlockDevice *device,
                       const char *mountpoint, MountFlags flags,
                       const char *options);
    Result<void> umount(const char *mountpoint);
    // 打开文件
    // 此处将会返回一个VFile payload, 其最终回收由VFS tidy_up负责
    Result<VFile *> open(const char *filepath);

    // 整理dentry_cache
    Result<void> tidy_up();

protected:
    // 将mnt_path的root更新到dentry_cache中
    Result<void> update_root(const util::Path &mnt_path);
    // locate到最近的位于dentry_cache/mount_table中的路径
    Result<DEntry *> locate(const util::Path &path);
    // 更新dentry_cache, 使其包含指定路径的dentry
    Result<void> update_dentry(const util::Path &path);

public:
    // 读取文件内容到buf中, 返回实际读取的字节数
    Result<size_t> read(VINode *vfile, off_t offset, void *buf,
                        size_t len) const;
    // 将buf中的内容写入文件, 返回实际写入的字节数
    Result<size_t> write(VINode *vfile, off_t offset, const void *buf,
                         size_t len) const;
    // 获取文件大小
    Result<size_t> size(VINode *vfile) const;
    // 刷新文件内容到存储设备
    Result<void> sync(VINode *vfile) const;
};
