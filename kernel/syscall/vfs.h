/**
 * @file vfs.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief VFS 相关系统调用
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/files.h>
#include <syscall/uaccess.h>

namespace syscall {
    [[nodiscard]]
    Result<CapIdx> vfs_open(CapIdx parent_dir_cap, const UString &relpath,
                            flags::oflg_t oflags);

    [[nodiscard]]
    Result<CapIdx> vfs_opendir(CapIdx parent_dir_cap, const UString &relpath,
                               flags::oflg_t oflags);
    [[nodiscard]]
    Result<CapIdx> vfs_mkfile(CapIdx parent_dir_cap, const UString &relpath,
                              flags::oflg_t oflags);
    [[nodiscard]]
    Result<CapIdx> vfs_mkdir(CapIdx parent_dir_cap, const UString &relpath,
                             flags::oflg_t oflags);

    [[nodiscard]]
    Result<size_t> vfs_read(CapIdx file_cap, size_t offset, UBuffer &&buf,
                            size_t len);

    [[nodiscard]]
    Result<size_t> vfs_write(CapIdx file_cap, size_t offset, UBuffer &&buf,
                             size_t len);

    [[nodiscard]]
    Result<size_t> vfs_size(CapIdx file_cap);

    [[nodiscard]]
    Result<size_t> vfs_getdents(CapIdx dir_cap, UBuffer &&buf, size_t buflen,
                                size_t offset);

    [[nodiscard]]
    Result<bool> vfs_sync(CapIdx capidx);

}  // namespace syscall
