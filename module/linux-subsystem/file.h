/**
 * @file file.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 文件操作
 * @version alpha-1.0.0
 * @date 2026-06-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>

size_t linux_sys_write(size_t fd, const void *buf, size_t len);
size_t linux_sys_read(int fd, void *buf, size_t count);
size_t linux_sys_writev(int fd, const void *iov, int iovcnt);
size_t linux_sys_readv(int fd, const void *iov, int iovcnt);
size_t linux_sys_close(int fd);
size_t linux_sys_dup(int oldfd);
size_t linux_sys_dup3(int oldfd, int newfd, int flags);
size_t linux_sys_fcntl(int fd, int cmd, size_t arg);
size_t linux_sys_ioctl(int fd, size_t request, size_t arg);
size_t linux_open_fd(const char *pathname, int fd, int flags);
size_t linux_opendir_fd(const char *pathname, int fd);
size_t linux_bind_cap_fd(CapIdx cap, int fd, bool append);
size_t linux_sys_openat(int dirfd, const char *pathname, int flags, int mode);
size_t linux_sys_lseek(int fd, size_t offset, int whence);
size_t linux_sys_ftruncate(int fd, size_t length);
size_t linux_sys_fallocate(int fd, int mode, size_t offset, size_t len);
size_t linux_sys_getcwd(char *buf, size_t size);
size_t linux_sys_chdir(const char *pathname);
size_t linux_sys_readlinkat(int dirfd, const char *pathname, char *buf,
                            size_t bufsiz);
size_t linux_sys_symlinkat(const char *target, int newdirfd,
                           const char *linkpath);
size_t linux_sys_mkdirat(int dirfd, const char *pathname, int mode);
size_t linux_sys_unlinkat(int dirfd, const char *pathname, int flags);
size_t linux_sys_renameat2(int olddirfd, const char *oldpath, int newdirfd,
                           const char *newpath, unsigned int flags);
size_t linux_sys_getdents64(int fd, void *dirp, size_t count);
size_t linux_sys_fstat(int fd, void *statbuf);
size_t linux_sys_statfs(const char *pathname, void *buf);
size_t linux_sys_fstatfs(int fd, void *buf);
size_t linux_sys_fchmodat(int dirfd, const char *pathname, uint32_t mode);
size_t linux_sys_fchownat(int dirfd, const char *pathname, uint32_t uid,
                          uint32_t gid, int flags);
size_t linux_sys_newfstatat(int dirfd, const char *pathname, void *statbuf,
                            int flags);
size_t linux_sys_statx(int dirfd, const char *pathname, int flags,
                       unsigned mask, void *statxbuf);
