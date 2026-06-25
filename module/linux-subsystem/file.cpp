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
#include <string>
#include <string_view>
#include <fdtable.h>

namespace {
    constexpr size_t INVALID_VALUE = 0xFFFF'FFFF'FFFF'FFFF;
    constexpr int AT_FDCWD         = -100;
    constexpr int LINUX_O_RDONLY   = 0;
    constexpr int LINUX_O_WRONLY   = 1;
    constexpr int LINUX_O_RDWR     = 2;
    constexpr int LINUX_O_CREAT    = 0100;   // octal
    constexpr int LINUX_O_DIRECTORY = 0200000;  // octal
    constexpr int AT_REMOVEDIR      = 0x200;
    constexpr size_t MAX_SEGMENTS   = 64;
    constexpr size_t MAX_DIR_FDS    = 128;
    constexpr uint8_t DT_REG        = 8;
    constexpr uint8_t DT_DIR        = 4;
    constexpr uint8_t DT_LNK        = 10;
    constexpr uint8_t DT_UNKNOWN    = 0;

    struct linux_dirent64 {
        uint64_t d_ino;
        int64_t d_off;
        unsigned short d_reclen;
        unsigned char d_type;
        char d_name[];
    };

    struct DirFdState {
        bool used         = false;
        int fd            = -1;
        size_t next_index = 0;
        std::string *abs_path = nullptr;
    };

    struct SplitPath {
        std::string parent{};
        std::string name{};
        bool valid = false;
    };

    DirFdState g_dir_fd_states[MAX_DIR_FDS]{};

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
    bool is_absolute_path(std::string_view path) noexcept {
        return !path.empty() && path.front() == '/';
    }

    [[nodiscard]]
    bool append_segment(std::string &out, size_t segment_starts[],
                        size_t &segment_count, std::string_view segment) {
        if (segment_count >= MAX_SEGMENTS) {
            return false;
        }
        size_t needed = out.size() + segment.size();
        if (out.size() > 1) {
            needed += 1;
        }
        if (needed >= LINUX_PATH_MAX) {
            return false;
        }

        if (out.size() > 1) {
            out.push_back('/');
        }
        segment_starts[segment_count++] = out.size();
        out.append(segment.data(), segment.size());
        return true;
    }

    [[nodiscard]]
    std::string normalize_absolute_path(std::string_view pathname) {
        if (!is_absolute_path(pathname)) {
            return {};
        }

        size_t segment_starts[MAX_SEGMENTS]{};
        size_t segment_count = 0;
        std::string out      = "/";

        size_t cursor = 0;
        while (cursor < pathname.size()) {
            while (cursor < pathname.size() && pathname[cursor] == '/') {
                ++cursor;
            }
            size_t segment_begin = cursor;
            while (cursor < pathname.size() && pathname[cursor] != '/') {
                ++cursor;
            }
            size_t segment_len = cursor - segment_begin;
            if (segment_len == 0) {
                break;
            }
            auto segment = pathname.substr(segment_begin, segment_len);
            if (segment == ".") {
                continue;
            }
            if (segment == "..") {
                if (segment_count == 0) {
                    continue;
                }
                size_t new_size = segment_starts[segment_count - 1];
                if (new_size > 1) {
                    --new_size;
                }
                out.resize(new_size);
                --segment_count;
                if (out.empty()) {
                    out = "/";
                }
                continue;
            }
            if (!append_segment(out, segment_starts, segment_count, segment)) {
                return {};
            }
        }

        return out.empty() ? "/" : out;
    }

    [[nodiscard]]
    std::string make_absolute_path(const char *pathname) {
        if (pathname == nullptr || pathname[0] == '\0') {
            return {};
        }
        if (is_absolute_path(pathname)) {
            return normalize_absolute_path(pathname);
        }

        std::string joined(__prog_cwd);
        if (joined.empty()) {
            joined = "/";
        }
        if (joined.size() > 1 && joined.back() != '/') {
            joined.push_back('/');
        }
        joined += pathname;
        if (joined.size() >= LINUX_PATH_MAX) {
            return {};
        }
        return normalize_absolute_path(joined);
    }

    [[nodiscard]]
    SplitPath split_parent_basename(std::string_view abs_path) {
        if (!is_absolute_path(abs_path)) {
            return {};
        }

        size_t last_slash = abs_path.rfind('/');
        if (last_slash == std::string_view::npos ||
            last_slash + 1 >= abs_path.size())
        {
            return {};
        }

        SplitPath split{};
        split.parent = last_slash == 0 ? "/" : std::string(abs_path.substr(0, last_slash));
        split.name   = std::string(abs_path.substr(last_slash + 1));
        split.valid  = !split.name.empty();
        if (!split.valid || split.parent.size() >= LINUX_PATH_MAX ||
            split.name.size() >= LINUX_PATH_MAX)
        {
            return {};
        }
        return split;
    }

    [[nodiscard]]
    CapIdx open_absolute_dir_cap(const std::string &abs_path) {
        if (!is_absolute_path(abs_path)) {
            return cap::error;
        }
        if (__prog_root_dir_cap == cap::null || __prog_root_dir_cap == cap::error) {
            return cap::error;
        }

        const char *relpath = abs_path.c_str() + 1;
        return sys_vfs_opendir(__prog_root_dir_cap, relpath, flags::O_READ);
    }

    [[nodiscard]]
    bool is_directory_absolute_path(const std::string &abs_path) {
        CapIdx dir_cap = open_absolute_dir_cap(abs_path);
        if (dir_cap == cap::null || dir_cap == cap::error) {
            return false;
        }
        sys_cap_remove(dir_cap);
        return true;
    }

    [[nodiscard]]
    size_t alloc_dir_fd_state(int fd, const std::string &abs_path) {
        if (fd < 0 || abs_path.empty() || abs_path.size() >= LINUX_PATH_MAX) {
            return static_cast<size_t>(-1);
        }
        for (size_t i = 0; i < MAX_DIR_FDS; ++i) {
            if (g_dir_fd_states[i].used) {
                continue;
            }
            g_dir_fd_states[i].used = true;
            g_dir_fd_states[i].fd = fd;
            g_dir_fd_states[i].next_index = 0;
            g_dir_fd_states[i].abs_path = new std::string(abs_path);
            return i;
        }
        return static_cast<size_t>(-1);
    }

    void clear_dir_fd_state(int fd) {
        for (auto &state : g_dir_fd_states) {
            if (!state.used || state.fd != fd) {
                continue;
            }
            delete state.abs_path;
            state = {};
            state.fd = -1;
            return;
        }
    }

    [[nodiscard]]
    DirFdState *find_dir_fd_state(int fd) {
        for (auto &state : g_dir_fd_states) {
            if (state.used && state.fd == fd) {
                return &state;
            }
        }
        return nullptr;
    }

    [[nodiscard]]
    uint8_t entry_type_to_dtype(EntryType type) noexcept {
        switch (type) {
            case EntryType::FILE:    return DT_REG;
            case EntryType::DIR:     return DT_DIR;
            case EntryType::SYMLINK: return DT_LNK;
            default:                 return DT_UNKNOWN;
        }
    }

    [[nodiscard]]
    size_t linux_dirent64_record_size(const char *name) {
        size_t name_len = strlen(name) + 1;
        size_t size = sizeof(linux_dirent64) + name_len;
        size_t align = alignof(uint64_t);
        return (size + align - 1) & ~(align - 1);
    }

    [[nodiscard]]
    bool encode_linux_dirent64(void *buf, size_t buflen, size_t &pos,
                               const char *name, const NodeMeta &meta,
                               size_t next_index) {
        size_t reclen = linux_dirent64_record_size(name);
        if (pos + reclen > buflen) {
            return false;
        }

        auto *entry = reinterpret_cast<linux_dirent64 *>(
            static_cast<char *>(buf) + pos);
        entry->d_ino = meta.inode;
        entry->d_off = static_cast<int64_t>(next_index);
        entry->d_reclen = static_cast<unsigned short>(reclen);
        entry->d_type = entry_type_to_dtype(meta.type);
        strcpy(entry->d_name, name);
        memset(reinterpret_cast<char *>(entry) + sizeof(linux_dirent64) +
                   strlen(name) + 1,
               0, reclen - (sizeof(linux_dirent64) + strlen(name) + 1));
        pos += reclen;
        return true;
    }

    [[nodiscard]]
    size_t do_open_absolute(const std::string &abs_path, int flags) {
        if (!is_absolute_path(abs_path)) {
            return -ENOENT;
        }
        if (__prog_root_dir_cap == cap::null || __prog_root_dir_cap == cap::error) {
            return -ENOENT;
        }

        flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);
        const char *relpath          = abs_path.c_str() + 1;
        bool want_directory = (flags & LINUX_O_DIRECTORY) != 0 ||
                              is_directory_absolute_path(abs_path);
        CapIdx file_cap     = want_directory
                                  ? sys_vfs_opendir(__prog_root_dir_cap, relpath,
                                                    sustcore_flags)
                                  : sys_vfs_open(__prog_root_dir_cap, relpath,
                                                 sustcore_flags);
        if (file_cap == cap::null || file_cap == cap::error) {
            loggers::LXSC::ERROR("Invalid path: %s", abs_path.c_str());
            return -ENOENT;
        }

        int fd = alloc_fd(file_cap);
        if (fd < 0) {
            sys_cap_remove(file_cap);
            return -EMFILE;
        }
        if (want_directory &&
            alloc_dir_fd_state(fd, abs_path) == static_cast<size_t>(-1))
        {
            free_fd(fd);
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
    if (find_dir_fd_state(fd) != nullptr) {
        return -EISDIR;
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
    clear_dir_fd_state(fd);
    free_fd(fd);
    return 0;
}

size_t linux_sys_openat(int dirfd, const char *pathname, int flags, int mode) {
    (void)mode;

    if (pathname == nullptr) {
        return -EINVAL;
    }

    std::string abs_path{};
    if (is_absolute_path(pathname) || dirfd == AT_FDCWD) {
        abs_path = make_absolute_path(pathname);
    } else {
        auto *dir_state = find_dir_fd_state(dirfd);
        if (dir_state == nullptr || dir_state->abs_path == nullptr) {
            return -EBADF;
        }
        std::string joined(*dir_state->abs_path);
        if (joined.size() > 1 && joined.back() != '/') {
            joined.push_back('/');
        }
        joined += pathname;
        if (joined.size() >= LINUX_PATH_MAX) {
            return -ENOENT;
        }
        abs_path = normalize_absolute_path(joined);
    }
    if (abs_path.empty()) {
        return -ENOENT;
    }
    return do_open_absolute(abs_path, flags);
}

size_t linux_sys_lseek(int fd, size_t offset, int whence) {
    auto *dir_state = find_dir_fd_state(fd);
    if (dir_state != nullptr) {
        switch (whence) {
            case 0:
                dir_state->next_index = offset;
                return dir_state->next_index;
            case 1:
                dir_state->next_index += offset;
                return dir_state->next_index;
            case 2:
            default:
                return -EINVAL;
        }
    }

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

        std::string cwd(__prog_cwd);
        size_t cwd_len = cwd.size() + 1;
        if (size < cwd_len) {
            return -ERANGE;
        }

        memcpy(buf, cwd.c_str(), cwd_len);
        return reinterpret_cast<size_t>(buf);
    }

size_t linux_sys_mkdirat(int dirfd, const char *pathname, int mode) {
    (void)mode;

    if (pathname == nullptr) {
        return -EINVAL;
    }
    if (dirfd != AT_FDCWD) {
        return -ENOSYS;
    }

    auto abs_path = make_absolute_path(pathname);
    auto split    = split_parent_basename(abs_path);
    if (abs_path.empty() || !split.valid) {
        return -EINVAL;
    }

    const char *relpath = abs_path.c_str() + 1;
    CapIdx dir_cap = sys_vfs_mkdir(__prog_root_dir_cap, relpath, flags::O_READ);
    if (dir_cap == cap::null || dir_cap == cap::error) {
        return -EIO;
    }
    sys_cap_remove(dir_cap);
    return 0;
}

size_t linux_sys_unlinkat(int dirfd, const char *pathname, int flags) {
    if (pathname == nullptr) {
        return -EINVAL;
    }
    if (dirfd != AT_FDCWD) {
        return -ENOSYS;
    }
    if ((flags & ~AT_REMOVEDIR) != 0) {
        return -EINVAL;
    }

    auto abs_path = make_absolute_path(pathname);
    auto split    = split_parent_basename(abs_path);
    if (abs_path.empty() || !split.valid) {
        return -EINVAL;
    }

    const char *relpath = abs_path.c_str() + 1;
    bool ok = (flags & AT_REMOVEDIR) != 0
                  ? sys_vfs_rmdir(__prog_root_dir_cap, relpath)
                  : sys_vfs_unlink(__prog_root_dir_cap, relpath);
    return ok ? 0 : -EIO;
}

size_t linux_sys_getdents64(int fd, void *dirp, size_t count) {
    if (dirp == nullptr && count != 0) {
        return -EFAULT;
    }
    if (count == 0) {
        return 0;
    }

    CapIdx dir_cap = fd_to_cap(fd);
    if (dir_cap == cap::error) {
        return -EBADF;
    }

    auto *dir_state = find_dir_fd_state(fd);
    size_t next_index = 0;
    if (dir_state != nullptr) {
        next_index = dir_state->next_index;
    }

    char raw_entries[LINUX_PATH_MAX]{};
    size_t raw_size =
        sys_vfs_getdents(dir_cap, raw_entries, sizeof(raw_entries), next_index);
    if (raw_size == INVALID_VALUE) {
        return -EIO;
    }
    if (raw_size == 0) {
        return 0;
    }

    size_t pos = 0;
    size_t raw_pos = 0;
    size_t entry_index = next_index;
    while (raw_pos + sizeof(dir_entry_header) <= raw_size) {
        auto *header =
            reinterpret_cast<const dir_entry_header *>(raw_entries + raw_pos);
        if (header->next_offset == 0 ||
            raw_pos + header->next_offset > raw_size)
        {
            break;
        }

        const char *name =
            raw_entries + raw_pos + sizeof(dir_entry_header);
        NodeMeta meta{};
        if (!sys_vfs_lstat(dir_cap, name, &meta)) {
            return -EIO;
        }
        if (!encode_linux_dirent64(dirp, count, pos, name, meta,
                                   entry_index + 1))
        {
            if (pos == 0) {
                return -EINVAL;
            }
            break;
        }

        raw_pos += header->next_offset;
        ++entry_index;
    }

    if (dir_state != nullptr) {
        dir_state->next_index = entry_index;
    }
    return pos;
}
