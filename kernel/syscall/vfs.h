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
#include <vfs/ops.h>

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

    [[nodiscard]]
    Result<void> vfs_unlink(CapIdx parent_dir_cap, const UString &relpath);
    [[nodiscard]]
    Result<void> vfs_rmdir(CapIdx parent_dir_cap, const UString &relpath);
    [[nodiscard]]
    Result<void> vfs_truncate(CapIdx file_cap, size_t new_size);
    [[nodiscard]]
    Result<void> vfs_rename(CapIdx old_parent_cap, const UString &old_name,
                            CapIdx new_parent_cap, const UString &new_name);
    [[nodiscard]]
    Result<void> vfs_symlink(CapIdx parent_dir_cap, const UString &relpath,
                             const UString &target);
    [[nodiscard]]
    Result<void> vfs_link(CapIdx parent_dir_cap, const UString &relpath,
                          CapIdx target_file_cap);
    [[nodiscard]]
    Result<void> vfs_stat(CapIdx parent_dir_cap, const UString &relpath,
                          UBuffer &&out);
    [[nodiscard]]
    Result<void> vfs_lstat(CapIdx parent_dir_cap, const UString &relpath,
                           UBuffer &&out);
    [[nodiscard]]
    Result<void> vfs_fstat(CapIdx file_cap, UBuffer &&out);
    [[nodiscard]]
    Result<size_t> vfs_readlink(CapIdx parent_dir_cap, const UString &relpath,
                                 UBuffer &&buf, size_t bufsiz);
    [[nodiscard]]
    Result<void> vfs_page_cache_stats(UBuffer &&out, bool reset);
    [[nodiscard]]
    Result<CapIdx> mnt_create(CapIdx devfile_cap, const UString &fs_name,
                              uint64_t superflags, const UString *options);
    [[nodiscard]]
    Result<bool> mnt_mount(CapIdx mntcap, CapIdx parent_dir_cap,
                           const UString &mountpoint, uint64_t attachflags);
    [[nodiscard]]
    Result<bool> mnt_umount(CapIdx mntcap, uint64_t flags);
    [[nodiscard]]
    Result<CapIdx> mnt_root(CapIdx mntcap);
    [[nodiscard]]
    MountStatus mnt_state(CapIdx mntcap);
    [[nodiscard]]
    Result<void> vfs_fchownat(CapIdx dirfd, const UString &relpath,
                              uint32_t uid, uint32_t gid, uint32_t flags);

}  // namespace syscall
