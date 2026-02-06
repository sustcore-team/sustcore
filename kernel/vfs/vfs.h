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
#include <vfs/ops.h>
#include <sus/map.h>
#include <sus/mstring.h>

typedef int fd_t;

enum class MountFlags { NONE = 0 };

// VFS File
class VFile {
public:
    fd_t fd;
    IFsDriver *fs;
    ISuperblock *sb;
    IFile *ifile;
};

class VFS {
private:
    fd_t fd_cnt;
    util::LinkedMap<util::string, IFsDriver *> fs_table;
    util::LinkedMap<util::string, ISuperblock *> mount_table;
    util::LinkedMap<fd_t, VFile *> open_file_list;
public:
    VFS();
    ~VFS();

    constexpr fd_t next_fd() noexcept {
        return fd_cnt++;
    }

    // 注册文件系统
    FSErrCode register_fs(IFsDriver *driver);
    FSErrCode unregister_fs(const char *fs_name);
    // 挂载文件系统
    FSErrCode mount(const char *fs_name, IBlockDevice *device,
                           const char *mountpoint, MountFlags flags,
                           const char *options);
    FSErrCode umount(const char *mountpoint);
    // 打开/关闭文件
    FSOptional<VFile *> _open(const char *path, int flags);
    FSErrCode _close(VFile *vfile);

    inline FSOptional<fd_t> open(const char *path, int flags) {
        auto _vfile_opt = _open(path, flags);
        return _vfile_opt.map<fd_t>([](VFile *vfile) {
            return vfile->fd;
        });
    }

    inline FSErrCode close(fd_t fd) {
        auto _vfile_opt = open_file_list.get(fd);
        if (! _vfile_opt.present()) {
            return FSErrCode::INVALID_PARAM;
        }
        VFile *vfile = _vfile_opt.value();
        return _close(vfile);
    }
};