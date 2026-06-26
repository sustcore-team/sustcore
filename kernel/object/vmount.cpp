/**
 * @file vmount.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief mount capability object
 * @version alpha-1.0.0
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <object/perm.h>
#include <object/vmount.h>

namespace cap {
    Result<bool> VMountObject::mount(VDirectory &parent, const char *mntpath,
                                     uint64_t attachflags) {
        using namespace perm::mount;
        if (!imply(MOUNT)) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        auto mount_res =
            VFS::inst().mount_attach(*_obj, parent, mntpath, attachflags);
        propagate(mount_res);
        return true;
    }

    Result<bool> VMountObject::umount(uint64_t flags) {
        using namespace perm::mount;
        if (!imply(MOUNT)) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        auto umount_res = VFS::inst().mount_detach(*_obj, flags);
        propagate(umount_res);
        return true;
    }

    Result<CapIdx> VMountObject::root(CHolder &holder) {
        using namespace perm::mount;
        if (!imply(ROOT)) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        return VFS::inst().mount_root(*_obj, holder);
    }

    Result<MountStatus> VMountObject::state() const {
        using namespace perm::mount;
        if (!imply(QUERY)) {
            loggers::CAPABILITY::ERROR("权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        return _obj->status();
    }
}  // namespace cap
