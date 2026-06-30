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
#include <cap/cholder.h>
#include <bio/blk.h>
#include <logger.h>
#include <spinlock.h>
#include <sus/nonnull.h>
#include <sus/owner.h>
#include <sus/path.h>
#include <sus/refcount.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>
#include <sustcore/files.h>
#include <vfs/device.h>
#include <vfs/ops.h>

#include <string>
#include <unordered_map>

class VFsDriver;
class VSuperblock;
class VINode;
class VFile;
class VDirectory;
class VMount;
class VFS;

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
    constexpr bool closable() const {
        return !alive();
    }

private:
    util::owner<ISuperblock *> _sb;
    util::refc_ptr<VFsDriver> _fsd;
    std::unordered_map<inode_t, VINode *> _inode_cache;

public:
    VSuperblock(util::owner<ISuperblock *> sb, VFsDriver &fsd)
        : _sb(sb), _fsd(&fsd) {}
    virtual ~VSuperblock();
    constexpr ISuperblock *sb() const {
        return _sb.get();
    }
    constexpr VFsDriver &vfsd() const {
        return *_fsd;
    }
    Result<util::refc_ptr<VINode>> get_vnode(inode_t inode_id);
    Result<void> invalidate_inode(inode_t inode_id);
    Result<void> evict_inode(inode_t inode_id);
    Result<void> flush_file_pages();
    void on_death();
};

class VINode : public util::refc<VINode> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::VFILE;

    struct CachedFilePage {
        PhyAddr paddr;
        size_t valid = 0;
        bool dirty = false;
        bool active = false;
        bool evicting = false;
        bool invalidating = false;
        VINode *owner = nullptr;
        size_t page_index = 0;
        CachedFilePage *prev = nullptr;
        CachedFilePage *next = nullptr;
        SpinLocker lock;
    };

private:

    util::owner<IINode *> _inode;
    util::refc_ptr<VFsDriver> _fsd;
    util::refc_ptr<VSuperblock> _vsb;
    std::unordered_map<size_t, CachedFilePage> _file_pages;
    size_t _cached_file_size     = 0;
    bool _cached_file_size_valid = false;

public:
    void on_death() {
        delete this;
    }

    Result<void> invalidate();

    [[nodiscard]]
    Result<PhyAddr> cached_file_page(IFile &file, size_t page_index,
                                     size_t *valid_len);
    [[nodiscard]]
    Result<size_t> read_cached_file(IFile &file, size_t offset, void *buf,
                                    size_t len);
    [[nodiscard]]
    Result<size_t> write_cached_file(IFile &file, size_t offset,
                                     const void *buf, size_t len);
    [[nodiscard]]
    Result<void> ensure_cached_file_size(IFile &file);
    [[nodiscard]]
    Result<size_t> merged_file_size();
    void reset_cached_file_size(size_t size) noexcept;
    void invalidate_cached_file_size() noexcept;
    [[nodiscard]]
    Result<void> flush_file_pages();
    [[nodiscard]]
    Result<bool> evict_file_page();
    [[nodiscard]]
    bool has_file_pages() const noexcept;
    void invalidate_file_pages() noexcept;

    constexpr IINode *inode() const {
        return _inode.get();
    }
    constexpr VFsDriver &vfsd() const {
        return *_fsd;
    }
    constexpr VSuperblock &superblock() const {
        return *_vsb;
    }

    VINode(util::owner<IINode *> inode, VFsDriver &fsd,
           VSuperblock &vsb)
        : _inode(inode), _fsd(&fsd), _vsb(&vsb) {}
    virtual ~VINode() {
        auto flush_res = flush_file_pages();
        if (!flush_res.has_value()) {
            loggers::VFS::ERROR("VINode 析构回写页缓存失败: inode=%u err=%s",
                                _inode.get() == nullptr ? 0U : static_cast<unsigned>(_inode->inode_id()),
                                to_cstring(flush_res.error()));
        }
        invalidate_file_pages();
        delete _inode;
    }
};

class VFile : public cap::_PayloadHelper<PayloadType::VFILE> {
private:
    util::refc_ptr<VINode> _vind;
    util::Path _mount_path;
    VFS *_vfs;

public:
    VFile(VINode &vind, const util::Path &mount_path, VFS &vfs);
    ~VFile() override = default;
    void destruct() override;

    [[nodiscard]]
    util::nonnull<VINode *> vinode() const {
        return util::nnullforce(_vind.get());
    }

    [[nodiscard]]
    const util::Path &mount_path() const {
        return _mount_path;
    }
};

class VDirectory : public cap::_PayloadHelper<PayloadType::VDIR> {
private:
    util::refc_ptr<VINode> _vind;
    util::Path _mount_path;
    util::Path _global_path;
    VFS *_vfs;

public:
    VDirectory(VINode &vind, const util::Path &mount_path,
               const util::Path &global_path, VFS &vfs);
    ~VDirectory() override = default;
    void destruct() override;

    [[nodiscard]]
    util::nonnull<VINode *> vinode() const {
        return util::nnullforce(_vind.get());
    }

    [[nodiscard]]
    const util::Path &mount_path() const {
        return _mount_path;
    }

    [[nodiscard]]
    const util::Path &global_path() const {
        return _global_path;
    }

    [[nodiscard]]
    VSuperblock &superblock() {
        return _vind->superblock();
    }

    [[nodiscard]]
    const VSuperblock &superblock() const {
        return _vind->superblock();
    }
};

class VMount : public cap::_PayloadHelper<PayloadType::VMOUNT> {
private:
    std::string _fs_name;
    uint64_t _superflags;
    std::string _options;
    bool _has_device;
    size_t _devno;
    MountStatus _status;
    VSuperblock *_active_vsb;
    VINode *_parent_vinode;
    std::string _entry_name;
    util::Path _mount_path;
    bool _is_block_mount;
    size_t _active_files;

public:
    VMount(std::string fs_name, uint64_t superflags, std::string options,
           bool has_device, size_t devno)
        : _fs_name(std::move(fs_name)),
          _superflags(superflags),
          _options(std::move(options)),
          _has_device(has_device),
          _devno(devno),
          _status(MountStatus::UMOUNTED),
          _active_vsb(nullptr),
          _parent_vinode(nullptr),
          _is_block_mount(false),
          _active_files(0) {}
    ~VMount() override = default;
    void destruct() override;

    [[nodiscard]]
    const std::string &fs_name() const {
        return _fs_name;
    }

    [[nodiscard]]
    uint64_t superflags() const noexcept {
        return _superflags;
    }

    [[nodiscard]]
    const std::string &options() const {
        return _options;
    }

    [[nodiscard]]
    bool has_device() const noexcept {
        return _has_device;
    }

    [[nodiscard]]
    size_t devno() const noexcept {
        return _devno;
    }

    [[nodiscard]]
    MountStatus status() const noexcept {
        return _status;
    }

    void set_status(MountStatus status) noexcept {
        _status = status;
    }

    [[nodiscard]]
    VSuperblock *active_vsb() const noexcept {
        return _active_vsb;
    }

    void set_active_vsb(VSuperblock *vsb) noexcept {
        _active_vsb = vsb;
    }

    [[nodiscard]]
    VINode *parent_vinode() const noexcept {
        return _parent_vinode;
    }

    void set_parent_vinode(VINode *parent_vinode) noexcept {
        _parent_vinode = parent_vinode;
    }

    [[nodiscard]]
    const std::string &entry_name() const noexcept {
        return _entry_name;
    }

    void set_entry_name(std::string entry_name) {
        _entry_name = std::move(entry_name);
    }

    [[nodiscard]]
    const util::Path &mount_path() const noexcept {
        return _mount_path;
    }

    void set_mount_path(util::Path mount_path) {
        _mount_path = std::move(mount_path);
    }

    [[nodiscard]]
    bool is_block_mount() const noexcept {
        return _is_block_mount;
    }

    void set_is_block_mount(bool is_block_mount) noexcept {
        _is_block_mount = is_block_mount;
    }

    [[nodiscard]]
    size_t active_files() const noexcept {
        return _active_files;
    }

    void set_active_files(size_t active_files) noexcept {
        _active_files = active_files;
    }

    void reset_active_mount_state() noexcept {
        _active_vsb = nullptr;
        _parent_vinode = nullptr;
        _entry_name.clear();
        _mount_path = {};
        _is_block_mount = false;
        _active_files = 0;
    }
};

enum class MountFlags { NONE = 0 };

class VFS {
public:
    struct MountView {
        std::string source;
        std::string target;
        std::string fsname;
        std::string options;
    };

    struct MountKey {
        VINode *parent;
        std::string entry;

        bool operator==(const MountKey &other) const {
            return parent == other.parent && entry == other.entry;
        }
    };

private:
    struct MountKeyHash {
        size_t operator()(const MountKey &key) const {
            return reinterpret_cast<size_t>(key.parent) ^
                   (std::hash<std::string>()(key.entry) << 1);
        }
    };

    struct MountRecord {
        VINode *parent_vinode = nullptr;
        std::string entry_name;
        util::Path mount_path;
        util::owner<VSuperblock *> superblock;
        size_t devno = 0;
        bool is_block_mount = false;
        size_t active_files = 0;
        VMount *owner_mount = nullptr;
    };

    std::unordered_map<std::string, util::owner<VFsDriver *>> fs_table;
    std::unordered_map<MountKey, MountRecord, MountKeyHash> mount_table;
    std::unordered_map<std::string, VSuperblock *> pseudo_mounts;

    [[nodiscard]]
    Result<util::refc_ptr<VINode>> _resolve_from(
        util::refc_ptr<VINode> base, const util::Path &base_path,
        const util::Path &path, VSuperblock *vsb) const;
    [[nodiscard]]
    Result<util::refc_ptr<VINode>> _resolve_path(
        util::refc_ptr<VINode> base, util::Path &mount_path,
        const util::Path &base_path, const util::Path &path, VSuperblock *vsb,
        size_t symlink_budget, bool follow_final_symlink = true) const;
    [[nodiscard]]
    Result<util::refc_ptr<VINode>> _follow_symlink(
        util::refc_ptr<VINode> symlink_vnode, const util::Path &mount_path,
        const util::Path &symlink_path, const util::Path &remaining_path,
        size_t symlink_budget, bool follow_final_symlink) const;

    [[nodiscard]]
    Result<VFile *> _open_file_at(VINode &parent, const util::Path &mount_path,
                                  const util::Path &base_path, const char *relpath,
                                  flags::oflg_t oflags);

    [[nodiscard]]
    Result<VDirectory *> _open_dir_at(VINode &parent,
                                      const util::Path &mount_path,
                                      const util::Path &base_path,
                                      const char *relpath);

    [[nodiscard]]
    Result<VFile *> _open_file(const char *filepath);

    [[nodiscard]]
    Result<VDirectory *> _open_dir(const char *filepath);

    [[nodiscard]]
    Result<MountRecord *> _lookup_mount_record(const MountKey &key) const;
    [[nodiscard]]
    Result<std::pair<MountKey, util::Path>> _build_mount_key(
        const util::Path &mount_path);
    [[nodiscard]]
    Result<MountKey> _entry_mount_key(VINode *parent, std::string_view entry) const;

    [[nodiscard]]
    Result<util::refc_ptr<VINode>> _resolve_inode(const util::Path &path,
                                                  util::Path &mount_path) const;
    [[nodiscard]]
    Result<util::refc_ptr<VINode>> _resolve_inode_no_follow(
        const util::Path &path, util::Path &mount_path) const;
    [[nodiscard]]
    Result<void> _stat_from_vinode(VINode &vnode, NodeMeta &out) const;
    [[nodiscard]]
    Result<std::pair<util::Path, util::Path>> _global_target_path(
        const VDirectory &base, const char *relpath) const;
    [[nodiscard]]
    std::vector<DirectoryEntryInfo> _append_mount_entries(
        const VDirectory &vdir, std::vector<DirectoryEntryInfo> entries)
        const;
    [[nodiscard]]
    Result<util::refc_ptr<VINode>> _resolve_parent_directory(
        util::refc_ptr<VINode> base, const util::Path &base_path,
        const util::Path &dir_path, VSuperblock *vsb, bool follow_symlink,
        bool create_intermediate_dirs);
    [[nodiscard]]
    Result<void> _ensure_mountpoint_path(const util::Path &mount_path);

    void _on_vfile_destroy(const util::Path &mount_path) noexcept;

public:
    static void init();
    static bool initialized();
    static VFS &inst();

    static VFSPageCacheStats page_cache_stats() noexcept;
    static void reset_page_cache_stats() noexcept;

    VFS() = default;
    ~VFS();

    VFS(const VFS &)            = delete;
    VFS &operator=(const VFS &) = delete;
    VFS(VFS &&)                 = delete;
    VFS &operator=(VFS &&)      = delete;

    // 注册文件系统
    Result<const char *> __register_fs(util::owner<IFsDriver *> &&driver);
    template <typename FsDriverClass>
    Result<const char *> register_fs()
    {
        return __register_fs(util::owner(new FsDriverClass()));
    }
    Result<void> unregister_fs(const char *fs_name);
    // 挂载文件系统
    Result<void> mount(const char *fs_name, size_t devno,
                       const char *mountpoint, MountFlags flags,
                       const char *options);
    Result<void> mount(const char *fs_name, const char *mountpoint,
                       const char *options);
    Result<void> umount(const char *mountpoint);
    Result<util::owner<VMount *>> create_mount(const char *fs_name,
                                               bool has_device, size_t devno,
                                               uint64_t superflags,
                                               const char *options);
    Result<void> mount_attach(VMount &mount, VDirectory &parent,
                              const char *mntpath, uint64_t attachflags);
    Result<void> mount_detach(VMount &mount, uint64_t flags);
    Result<CapIdx> mount_root(VMount &mount, cap::CHolder &holder);
    Result<void> fstat(cap::Capability &cap, NodeMeta &out) const;
    // 打开文件并直接插入到指定 holder 中
    Result<CapIdx> open(const char *filepath, cap::CHolder &holder);
    [[nodiscard]]
    Result<CapIdx> open(cap::Capability &parent_dir_cap, const char *relpath,
                        flags::oflg_t oflags, cap::CHolder &holder);
    /**
     * @brief 强制以给定权限打开文件并插入 holder.
     *
     * 当前实现与普通 open 基本一致, 但调用方可显式指定发放给 holder 的
     * capability 权限. 未来该接口将用于 pseudo fs / procfs 这类内核自控对象
     * 的能力发放路径, 不对“目标应该拥有哪些权限”做额外判定, 而是始终按
     * 调用者给定权限发放 capability.
     */
    [[nodiscard]]
    Result<CapIdx> __force_open(cap::Capability &parent_dir_cap,
                                const char *relpath, flags::oflg_t oflags,
                                b64 perm, cap::CHolder &holder);
    /**
     * @brief 强制以给定权限打开绝对路径文件并插入 holder.
     *
     * 当前实现复用普通绝对路径文件解析逻辑, 但最终 capability 权限不由
     * open flags 推导, 而是始终按调用方提供的 perm 发放.
     */
    [[nodiscard]]
    Result<CapIdx> __force_open(const char *filepath, b64 perm,
                                cap::CHolder &holder);
    [[nodiscard]]
    Result<CapIdx> opendir(cap::Capability &parent_dir_cap, const char *relpath,
                           flags::oflg_t oflags, cap::CHolder &holder);
    [[nodiscard]]
    Result<CapIdx> mkfile(cap::Capability &parent_dir_cap, const char *relpath,
                          flags::oflg_t oflags, cap::CHolder &holder);
    [[nodiscard]]
    Result<CapIdx> mkdir(cap::Capability &parent_dir_cap, const char *relpath,
                         flags::oflg_t oflags, cap::CHolder &holder);
    [[nodiscard]]
    Result<void> unlink(cap::Capability &parent_dir_cap,
                        const char *relpath);
    [[nodiscard]]
    Result<void> rmdir(cap::Capability &parent_dir_cap,
                       const char *relpath);
    [[nodiscard]]
    Result<void> truncate(cap::Capability &file_cap, size_t new_size);
    [[nodiscard]]
    Result<void> ioctl(VFile &vfile, size_t cmd, syscall::UBuffer &&arg);
    [[nodiscard]]
    Result<void> link(cap::Capability &parent_dir_cap,
                      const char *relpath,
                      cap::Capability &target_inode_cap);
    [[nodiscard]]
    Result<void> rename(cap::Capability &old_parent_cap,
                        const char *old_name,
                        cap::Capability &new_parent_cap,
                        const char *new_name);
    [[nodiscard]]
    Result<void> symlink(cap::Capability &parent_dir_cap,
                         const char *relpath, const char *target);
    [[nodiscard]]
    Result<void> stat(cap::Capability &parent_dir_cap, const char *relpath,
                      NodeMeta &out) const;
    [[nodiscard]]
    Result<void> lstat(cap::Capability &parent_dir_cap, const char *relpath,
                       NodeMeta &out) const;
    [[nodiscard]]
    Result<void> statfs(cap::Capability &cap, VFSStatFS &out) const;
    [[nodiscard]]
    Result<void> statfs_at(cap::Capability &parent_dir_cap, const char *relpath,
                           VFSStatFS &out) const;
    [[nodiscard]]
    Result<void> getattr(cap::Capability &cap, AttrSet &out) const;
    [[nodiscard]]
    Result<void> getattr_at(cap::Capability &parent_dir_cap,
                            const char *relpath, AttrSet &out,
                            uint32_t flags) const;
    [[nodiscard]]
    Result<void> setattr(cap::Capability &cap, AttrMask mask,
                         const AttrSet &attrs) const;
    [[nodiscard]]
    Result<void> setattr_at(cap::Capability &parent_dir_cap,
                            const char *relpath, AttrMask mask,
                            const AttrSet &attrs, uint32_t flags) const;
    [[nodiscard]]
    Result<void> chown(cap::Capability &cap, uint32_t uid, uint32_t gid,
                       uint32_t flags) const;
    [[nodiscard]]
    Result<void> chown_at(cap::Capability &parent_dir_cap, const char *relpath,
                          uint32_t uid, uint32_t gid, uint32_t flags) const;
    [[nodiscard]]
    Result<size_t> readlink(cap::Capability &parent_dir_cap, const char *relpath,
                            char *buf, size_t bufsiz) const;
    [[nodiscard]]
    Result<CapIdx> open_dir(const char *filepath, cap::CHolder &holder,
                            b64 perm);
    [[nodiscard]]
    Result<ISuperblock *> get_pseudo(const char *pseudo_fs_id);
    [[nodiscard]]
    std::vector<MountView> snapshot_mounts() const;
    [[nodiscard]]
    Result<devfs::DevFSSuperblock *> devfs();
    // 仅供测试代码使用的调试接口
    Result<VFile *> __debug_open(const char *filepath);

public:
    // 读取文件内容到buf中, 返回实际读取的字节数
    Result<size_t> read(VFile &vfile, off_t offset, void *buf, size_t len) const;
    // 将buf中的内容写入文件, 返回实际写入的字节数
    Result<size_t> write(VFile &vfile, off_t offset, const void *buf,
                         size_t len) const;
    // 获取文件大小
    Result<size_t> size(VFile &vfile) const;
    // 刷新文件内容到存储设备
    Result<void> sync(VFile &vfile) const;
    Result<std::vector<DirectoryEntryInfo>> getdents(VDirectory &vdir) const;
    Result<void> sync(VDirectory &vdir) const;

    friend class VFile;
    friend class VDirectory;
};
