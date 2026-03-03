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

#include <device/block.h>
#include <sus/owner.h>
#include <sus/map.h>
#include <sus/mstring.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

enum class MountFlags { NONE = 0 };

class VFS {
private:
    util::LinkedMap<std::string, util::owner<IFsDriver *>> fs_table;
    util::LinkedMap<std::string, util::owner<ISuperblock *>> mount_table;
public:
    VFS() = default;
    ~VFS() = default;

    VFS(const VFS&)            = delete;
    VFS& operator=(const VFS&) = delete;
    VFS(VFS&&)                 = delete;
    VFS& operator=(VFS&&)      = delete;

    // 注册文件系统
    FSErrCode register_fs(util::owner<IFsDriver *> &&driver);
    FSErrCode unregister_fs(const char *fs_name);
    // 挂载文件系统
    FSErrCode mount(const char *fs_name, IBlockDevice *device,
                    const char *mountpoint, MountFlags flags,
                    const char *options);
    FSErrCode umount(const char *mountpoint);
};