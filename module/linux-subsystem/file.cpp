/**
 * @file file.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 文件操作
 * @version alpha-1.0.0
 * @date 2026-06-23
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <file.h>

#include <errno.h>
#include <logger.h>
#include <prog.h>
#include <syscall.h>

#include <cstddef>
#include <cstring>

#include "fdtable.h"

namespace {
    constexpr size_t INVALID_VALUE = 0xFFFF'FFFF'FFFF'FFFF;
    constexpr int AT_FDCWD         = -100;
    constexpr int LINUX_O_RDONLY   = 0;
    constexpr int LINUX_O_WRONLY   = 1;
    constexpr int LINUX_O_RDWR     = 2;
    constexpr int LINUX_O_CREAT    = 0100;   // octal
    constexpr size_t MAX_SEGMENTS  = 64;

    [[nodiscard]]
    flags::oflg_t linux_oflags_to_sustcore(int linux_flags) noexcept {
        flags::oflg_t result = 0;
        int access_mode      = linux_flags & 3;

        if (access_mode == LINUX_O_RDONLY) {
            result = flags::O_READ;
        } else if (access_mode == LINUX_O_WRONLY) {
            result = flags::O_WRITE;
        } else if (access_mode == LINUX_O_RDWR) {
            result = flags::O_READ | flags::O_WRITE;
        }

        if ((linux_flags & LINUX_O_CREAT) != 0) {
            result |= flags::O_CREAT;
        }

        return result;
    }

    [[nodiscard]]
    bool is_absolute_path(const char *path) noexcept {
        return path != nullptr && path[0] == '/';
    }

    [[nodiscard]]
    bool append_segment(char *out, size_t out_size, size_t &out_len,
                        size_t segment_starts[], size_t &segment_count,
                        const char *segment, size_t segment_len) {
        size_t needed = out_len;
        if (out_len > 1) {
            needed += 1;
        }
        needed += segment_len + 1;
        if (needed > out_size || segment_count >= MAX_SEGMENTS) {
            return false;
        }

        if (out_len > 1) {
            out[out_len++] = '/';
        }
        segment_starts[segment_count++] = out_len;
        memcpy(out + out_len, segment, segment_len);
        out_len += segment_len;
        out[out_len] = '\0';
        return true;
    }

    [[nodiscard]]
    bool normalize_absolute_path(const char *pathname, char *out,
                                 size_t out_size) {
        if (pathname == nullptr || pathname[0] != '/' || out == nullptr ||
            out_size < 2)
        {
            return false;
        }

        size_t out_len                    = 1;
        size_t segment_starts[MAX_SEGMENTS]{};
        size_t segment_count              = 0;
        out[0]                            = '/';
        out[1]                            = '\0';

        const char *cursor = pathname;
        while (*cursor != '\0') {
            while (*cursor == '/') {
                ++cursor;
            }
            const char *segment_begin = cursor;
            while (*cursor != '\0' && *cursor != '/') {
                ++cursor;
            }
            size_t segment_len = static_cast<size_t>(cursor - segment_begin);
            if (segment_len == 0) {
                break;
            }
            if (segment_len == 1 && segment_begin[0] == '.') {
                continue;
            }
            if (segment_len == 2 && segment_begin[0] == '.' &&
                segment_begin[1] == '.')
            {
                if (segment_count == 0) {
                    continue;
                }
                out_len = segment_starts[segment_count - 1];
                if (out_len > 1) {
                    --out_len;
                }
                out[out_len] = '\0';
                --segment_count;
                if (out_len == 0) {
                    out[0]  = '/';
                    out[1]  = '\0';
                    out_len = 1;
                }
                continue;
            }
            if (!append_segment(out, out_size, out_len, segment_starts,
                                segment_count, segment_begin, segment_len))
            {
                return false;
            }
        }

        if (out_len == 0) {
            out[0] = '/';
            out[1] = '\0';
        }
        return true;
    }

    [[nodiscard]]
    bool make_absolute_path(const char *pathname, char *out, size_t out_size) {
        if (pathname == nullptr || pathname[0] == '\0' || out == nullptr ||
            out_size < 2)
        {
            return false;
        }
        if (is_absolute_path(pathname)) {
            return normalize_absolute_path(pathname, out, out_size);
        }

        char joined[LINUX_PATH_MAX]{};
        size_t cwd_len  = strlen(__prog_cwd);
        size_t path_len = strlen(pathname);
        size_t total    = cwd_len + (cwd_len > 1 ? 1 : 0) + path_len + 1;
        if (total > sizeof(joined)) {
            return false;
        }

        memcpy(joined, __prog_cwd, cwd_len);
        size_t pos = cwd_len;
        if (pos > 1 && joined[pos - 1] != '/') {
            joined[pos++] = '/';
        }
        memcpy(joined + pos, pathname, path_len + 1);
        return normalize_absolute_path(joined, out, out_size);
    }

    [[nodiscard]]
    size_t do_open_absolute(const char *abs_path, int flags) {
        if (!is_absolute_path(abs_path)) {
            return -ENOENT;
        }
        if (__prog_root_dir_cap == cap::null || __prog_root_dir_cap == cap::error) {
            return -ENOENT;
        }

        flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);
        const char *relpath          = abs_path + 1;
        CapIdx file_cap = sys_vfs_open(__prog_root_dir_cap, relpath, sustcore_flags);
        if (file_cap == cap::null || file_cap == cap::error) {
            loggers::LXSC::ERROR("Invalid path: %s", abs_path);
            return -ENOENT;
        }

        int fd = alloc_fd(file_cap);
        if (fd < 0) {
            sys_cap_remove(file_cap);
            return -EMFILE;
        }
        return static_cast<size_t>(fd);
    }
}  // namespace

size_t linux_sys_write(size_t fd, const void *buf, size_t len) {
    if (fd == 1 || fd == 2) {
        sys_write_serial(0, reinterpret_cast<const char *>(buf), len);
        return len;
    }

    if (buf == nullptr) {
        return -EFAULT;
    }

    CapIdx file_cap = fd_to_cap(static_cast<int>(fd));
    if (file_cap == cap::error) {
        return -EBADF;
    }

    size_t offset  = fd_offset(static_cast<int>(fd));
    size_t written = sys_vfs_write(file_cap, offset,
                                   reinterpret_cast<const void *>(buf), len);
    if (written == INVALID_VALUE) {
        return -EIO;
    }

    set_fd_offset(static_cast<int>(fd), offset + written);
    return written;
}

size_t linux_sys_read(int fd, void *buf, size_t count) {
    if (fd == 0) {
        return -EBADF;
    }
    if (buf == nullptr && count != 0) {
        return -EFAULT;
    }
    if (count == 0) {
        return 0;
    }

    CapIdx file_cap = fd_to_cap(fd);
    if (file_cap == cap::error) {
        return -EBADF;
    }

    size_t offset = fd_offset(fd);
    size_t nread  = sys_vfs_read(file_cap, offset, buf, count);
    if (nread == INVALID_VALUE) {
        return -EIO;
    }

    set_fd_offset(fd, offset + nread);
    return nread;
}

size_t linux_sys_close(int fd) {
    CapIdx cap = fd_to_cap(fd);
    if (cap == cap::error) {
        return -EBADF;
    }
    free_fd(fd);
    return 0;
}

size_t linux_sys_openat(int dirfd, const char *pathname, int flags, int mode) {
    (void)mode;

    if (pathname == nullptr) {
        return -EINVAL;
    }
    if (dirfd != AT_FDCWD) {
        return -ENOSYS;
    }

    char abs_path[LINUX_PATH_MAX]{};
    if (!make_absolute_path(pathname, abs_path, sizeof(abs_path))) {
        return -ENOENT;
    }
    return do_open_absolute(abs_path, flags);
}

size_t linux_sys_lseek(int fd, size_t offset, int whence) {
    CapIdx file_cap = fd_to_cap(fd);
    if (file_cap == cap::error) {
        return -EBADF;
    }

    size_t new_offset = 0;
    switch (whence) {
        case 0:
            new_offset = offset;
            break;
        case 1:
            new_offset = fd_offset(fd) + offset;
            break;
        case 2: {
            size_t file_size = sys_vfs_size(file_cap);
            if (file_size == static_cast<size_t>(-1)) {
                return -EIO;
            }
            new_offset = file_size + offset;
            break;
        }
        default:
            return -EINVAL;
    }

    set_fd_offset(fd, new_offset);
    return new_offset;
}

size_t linux_sys_getcwd(char *buf, size_t size) {
    if (buf == nullptr || size == 0) {
        return -EINVAL;
    }

    size_t cwd_len = strlen(__prog_cwd) + 1;
    if (size < cwd_len) {
        return -ERANGE;
    }

    memcpy(buf, __prog_cwd, cwd_len);
    return reinterpret_cast<size_t>(buf);
}
