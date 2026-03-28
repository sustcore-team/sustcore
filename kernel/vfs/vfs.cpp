/**
 * @file vfs.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sus/owner.h>
#include <sus/path.h>
#include <sus/raii.h>
#include <sustcore/errcode.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cstring>
#include <expected>

Result<IDirectory *> IINode::as_directory() {
    IDirectory *dir = this->as<IDirectory>();
    if (dir) {
        return dir;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

Result<IFile *> IINode::as_file() {
    IFile *file = this->as<IFile>();
    if (file) {
        return file;
    } else {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
}

Result<void> VFS::register_fs(util::owner<IFsDriver *> &&driver) {
    const char *fs_name = driver->name();
    if (fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    fs_table.put(fs_name, util::owner(new VFsDriver(std::move(driver))));
    return {};
}

Result<void> VFS::unregister_fs(const char *fs_name) {
    if (!fs_table.contains(fs_name)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto get_res = fs_table.get(fs_name);
    if (!get_res.has_value()) {
        unexpect_return(ErrCode::UNKNOWN_ERROR);
    }

    auto tidy_res = tidy_up();
    propagate(tidy_res);

    // 未被引用: not busy, 可以删除
    util::owner<VFsDriver *> driver = get_res.value();
    if (!driver->closable()) {
        unexpect_return(ErrCode::BUSY);
    }

    fs_table.remove(fs_name);
    delete driver;
    void_return();
}

Result<void> VFS::mount(const char *fs_name, IBlockDevice *device,
                        const char *mountpoint, MountFlags flags,
                        const char *options) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    if (mount_table.contains(mnt_path)) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 已经被挂载
    }
    // 否则, 挂载该文件系统
    // 查找文件系统驱动
    auto lookup_result = fs_table.get(fs_name);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 未注册该文件系统
    }
    // 获取文件系统驱动
    VFsDriver *fsd = lookup_result.value();

    // 挂载文件系统
    auto mount_result = fsd->fsd()->mount(device, options);
    return mount_result.transform(
        [this, mnt_path, fsd](util::owner<ISuperblock *> isb) {
            util::owner<VSuperblock *> vsb =
                util::owner(new VSuperblock(isb, *fsd));
            this->mount_table.put(mnt_path, vsb);
        });
}

Result<void> VFS::umount(const char *mountpoint) {
    util::Path mnt_path = util::Path::normalize(mountpoint);
    auto lookup_result  = mount_table.get_entry(mnt_path);
    if (!lookup_result.has_value()) {
        unexpect_return(ErrCode::INVALID_PARAM);  // 没有该挂载点
    }

    util::owner<VSuperblock *> vsb = lookup_result.value().second;

    // 确定没有打开的文件属于该挂载点
    // 先进行一次tidy_up, 清理掉已亡文件
    // 因为我们并不会在文件的引用计数归零时即刻对其进行删除
    Result<void> res = tidy_up();
    if (!res.has_value()) {
        return res;
    }

    // 若仍有该超级块上的 inode 存在, 视为 busy, 禁止卸载
    if (vsb->busy()) {
        unexpect_return(ErrCode::BUSY);
    }

    // 移除挂载
    this->mount_table.remove(mnt_path);
    Result<void> ret = vsb->vfsd().fsd()->unmount(vsb->sb());
    delete vsb;

    return ret;
}

Result<util::owner<VFileAccessor *>> VFS::open(const char *filepath) {
    util::Path path = util::Path::from(filepath).normalize();

    // try update dentry cache
    auto update_res = update_dentry(path);
    propagate(update_res);

    // get virtual inode from dentry
    auto get_res = dentry_cache.get(path);
    assert(get_res.has_value());
    VINode *vind = get_res.value()->vind();
    return util::owner(new VFileAccessor(vind));
}

Result<void> VFS::tidy_up() {
    // 遍历 dentry_cache，记录可以删除的 dentry
    util::ArrayList<util::Path> closable_list;
    for (const auto &entry : dentry_cache) {
        DEntry *dentry = entry.second.get();
        if (dentry->closable()) {
            closable_list.push_back(entry.first);
        }
    }

    // 逐个从 dentry_cache 中移除并释放
    for (const auto &closable_path : closable_list) {
        auto get_res = dentry_cache.get(closable_path);
        if (!get_res.has_value()) {
            unexpect_return(ErrCode::FS_ERROR);
        }

        util::owner<DEntry *> dentry_owner = get_res.value();
        dentry_cache.remove(closable_path);
        delete dentry_owner;
    }

    // 最后遍历mount_table, 对每个挂载点的inode_cache进行整理
    for (const auto &entry : mount_table) {
        VSuperblock *vsb = entry.second.get();
        auto tidy_res = vsb->tidy_up();
        if (!tidy_res.has_value()) {
            unexpect_return(ErrCode::FS_ERROR);
        }
    }

    void_return();
}

// 整理inode_cache
Result<void> VSuperblock::tidy_up() {
    // 遍历 inode_cache，记录可以删除的 inode
    util::ArrayList<inode_t> closable_list;
    for (const auto &entry : inode_cache) {
        VINode *vind = entry.second.get();
        if (vind->closable()) {
            closable_list.push_back(entry.first);
        }
    }

    // 逐个从 inode_cache 中移除并释放
    for (const auto &closable_inode : closable_list) {
        auto get_res = inode_cache.get(closable_inode);
        if (!get_res.has_value()) {
            unexpect_return(ErrCode::FS_ERROR);
        }

        util::owner<VINode *> vinode_owner = get_res.value();
        inode_cache.remove(closable_inode);
        delete vinode_owner;
    }

    void_return();
}

// 更新inode_cache, 使其包含指定inode号的inode
Result<VINode *> VSuperblock::update_inode(inode_t inode_id) {
    if (inode_cache.contains(inode_id)) {
        return inode_cache.get(inode_id)
            .transform(std::mem_fn(&util::owner<VINode *>::get))
            .transform_error(always(ErrCode::FS_ERROR));
    }

    // 如果未命中则构造
    auto get_res = sb()->get_inode(inode_id);
    propagate(get_res);

    util::owner<VINode *> vind =
        util::owner(new VINode(get_res.value(), *_fsd, *this));
    if (!vind) {
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }
    inode_cache.put(inode_id, vind);
    return vind.get();
}

// 将mnt_path的root更新到dentry_cache中
Result<void> VFS::update_root(const util::Path &mnt_path) {
    if (! mount_table.contains(mnt_path)) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    auto get_res = mount_table.get(mnt_path);
    if (!get_res.has_value()) {
        unexpect_return(ErrCode::UNKNOWN_ERROR);
    }
    VSuperblock *vsb = get_res.value();
    auto root_res = vsb->sb()->root().and_then(
        [vsb](inode_t root_id) { return vsb->update_inode(root_id); }
    );
    propagate(root_res);
    VINode *root_vind = root_res.value();
    util::owner<DEntry *> root_de =
        util::owner(new DEntry(mnt_path, root_vind, nullptr));
    if (!root_de) {
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }
    assert (! dentry_cache.contains(mnt_path));
    dentry_cache.put(mnt_path, root_de);
    void_return();
}

// locate到最近的位于dentry_cache/mount_table中的路径
Result<DEntry *> VFS::locate(const util::Path &path) {
    // 沿着path向上寻找, 直到找到一个在dentry_cache中的路径
    // 然后沿着这个路径向下更新dentry_cache, 直到path被包含在内
    util::Path cur_path = path;
    bool rooted = false;
    while (! rooted) {
        if (cur_path == "/") {
            rooted = true;
        }

        // 找到一个起点
        if (dentry_cache.contains(cur_path)) {
            break;
        }

        if (mount_table.contains(cur_path)) {
            auto update_res = update_root(cur_path);
            if (!update_res.has_value()) {
                // 判断错误类型
                // 目前先返回错误
                // 可以根据错误类型修复mount_table, 或继续向上查找
                propagate_return(update_res);
            } else {
                break;
            }
        }

        cur_path = cur_path.parent_path();
    }
    return dentry_cache.get(cur_path)
        .transform(std::mem_fn(&util::owner<DEntry *>::get))
        .transform_error(always(ErrCode::INVALID_PARAM));
}

// 更新dentry_cache, 使其包含指定路径的dentry
Result<void> VFS::update_dentry(const util::Path &path) {
    if (dentry_cache.contains(path)) {
        void_return();
    }

    auto locate_res = locate(path);
    propagate(locate_res);
    DEntry *base_dentry      = locate_res.value();
    VINode *root             = base_dentry->vind();
    VSuperblock &vsb         = root->superblock();
    const util::Path relpath = path.relative_to(base_dentry->path());

    // 沿着root向下走
    VINode *curnd       = root;
    DEntry *parde       = base_dentry;
    util::Path walkpath = base_dentry->path();
    for (const auto &entry : relpath) {
        auto lookup_res = curnd->inode()->as_directory().and_then(
            [entry](IDirectory *dir) { return dir->lookup(entry); });
        propagate(lookup_res);

        // update inode cache
        inode_t nxtid = lookup_res.value();
        auto upd_res  = vsb.update_inode(nxtid);
        propagate(upd_res);

        // iterate to next node
        curnd    = upd_res.value();
        walkpath = walkpath / entry;
        walkpath = walkpath.normalize();
        util::owner<DEntry *> nxtde =
            util::owner(new DEntry(walkpath, curnd, parde));
        parde = nxtde.get();

        // update dentry cache
        dentry_cache.put(walkpath, nxtde);
    }

    // 如此一来, 就更新了dentry_cache, 使其包含path
    void_return();
}