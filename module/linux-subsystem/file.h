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
size_t linux_sys_close(int fd);
size_t linux_sys_openat(int dirfd, const char *pathname, int flags, int mode);
size_t linux_sys_lseek(int fd, size_t offset, int whence);
size_t linux_sys_getcwd(char *buf, size_t size);
size_t linux_sys_mkdirat(int dirfd, const char *pathname, int mode);
size_t linux_sys_unlinkat(int dirfd, const char *pathname, int flags);
size_t linux_sys_getdents64(int fd, void *dirp, size_t count);
