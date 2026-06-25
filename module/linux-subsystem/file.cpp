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
#include <fdtable.h>
#include <logger.h>
#include <prog.h>
#include <sus/path.h>
#include <syscall.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <utility>

namespace {
    constexpr size_t INVALID_VALUE = 0xFFFF'FFFF'FFFF'FFFF;
    constexpr int AT_FDCWD         = -100;
    constexpr int LINUX_O_RDONLY   = 0;
    constexpr int LINUX_O_WRONLY   = 1;
    constexpr int LINUX_O_RDWR     = 2;
    constexpr int LINUX_O_CREAT    = 0100;   // octal
    constexpr int LINUX_O_DIRECTORY = 0200000;  // octal
    constexpr int AT_REMOVEDIR      = 0x200;
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
        bool pinned       = false;
        int fd            = -1;
        size_t next_index = 0;
        std::string *abs_path = nullptr;
    };

    struct DirBase {
        CapIdx cap = cap::null;
        std::string abs_path{};
    };

    struct ResolvedPath {
        CapIdx parent_cap = cap::null;
        std::string absolute_path{};
        std::string relative_path{};
    };

    enum class ResolvedNodeType {
        FILE,
        DIRECTORY,
        MISSING,
        ERROR,
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
    util::Path normalize_path(const util::Path &path) {
        return path.normalize();
    }

    [[nodiscard]]
    std::string path_to_string(const util::Path &path) {
        return static_cast<std::string>(path);
    }

    [[nodiscard]]
    util::Path make_path(const char *pathname) {
        return util::Path::from(pathname == nullptr ? "" : pathname);
    }

    [[nodiscard]]
    bool valid_normalized_path(const util::Path &path) {
        auto text = path.view();
        return !text.empty() && text.size() < LINUX_PATH_MAX;
    }

    [[nodiscard]]
    std::string absolute_path_to_relpath(const util::Path &abs_path) {
        auto normalized = normalize_path(abs_path);
        if (!normalized.is_absolute()) {
            return {};
        }
        auto text = path_to_string(normalized);
        if (text == "/") {
            return ".";
        }
        return text.substr(1);
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

    void free_dir_fd_state(DirFdState &state) {
        delete state.abs_path;
        state = {};
        state.fd = -1;
    }

    [[nodiscard]]
    size_t register_dir_fd_state(int fd, const std::string &abs_path,
                                 bool pinned) {
        if (fd < 0 || abs_path.empty() || abs_path.size() >= LINUX_PATH_MAX) {
            return static_cast<size_t>(-1);
        }

        if (auto *state = find_dir_fd_state(fd); state != nullptr) {
            state->next_index = 0;
            state->pinned     = pinned;
            if (state->abs_path == nullptr) {
                state->abs_path = new std::string(abs_path);
            } else {
                *state->abs_path = abs_path;
            }
            return 0;
        }

        for (size_t i = 0; i < MAX_DIR_FDS; ++i) {
            if (g_dir_fd_states[i].used) {
                continue;
            }
            g_dir_fd_states[i].used       = true;
            g_dir_fd_states[i].pinned     = pinned;
            g_dir_fd_states[i].fd         = fd;
            g_dir_fd_states[i].next_index = 0;
            g_dir_fd_states[i].abs_path   = new std::string(abs_path);
            return i;
        }
        return static_cast<size_t>(-1);
    }

    void clear_dir_fd_state(int fd) {
        auto *state = find_dir_fd_state(fd);
        if (state == nullptr || state->pinned) {
            return;
        }
        free_dir_fd_state(*state);
    }

    [[nodiscard]]
    CapIdx open_dir_cap_at(CapIdx parent_cap, const std::string &relative_path) {
        if (parent_cap == cap::null || parent_cap == cap::error) {
            return cap::error;
        }
        return sys_vfs_opendir(parent_cap, relative_path.c_str(), flags::O_READ);
    }

    [[nodiscard]]
    bool ensure_cwd_fd_bound() {
        if (__prog_cwd_dir_cap == cap::null || __prog_cwd_dir_cap == cap::error) {
            return false;
        }

        auto current_cap = fd_to_cap(CWD_FD);
        auto *entry      = lookup_fd(CWD_FD);
        auto *state      = find_dir_fd_state(CWD_FD);
        if (current_cap == __prog_cwd_dir_cap && entry != nullptr &&
            state != nullptr && state->abs_path != nullptr &&
            *state->abs_path == __prog_cwd)
        {
            entry->offset = 0;
            state->next_index = 0;
            state->pinned     = true;
            return true;
        }

        if (!bind_fd(CWD_FD, __prog_cwd_dir_cap)) {
            return false;
        }

        entry = lookup_fd(CWD_FD);
        if (entry == nullptr) {
            return false;
        }
        entry->cap    = __prog_cwd_dir_cap;
        entry->offset = 0;
        return register_dir_fd_state(CWD_FD, __prog_cwd, true) !=
               static_cast<size_t>(-1);
    }

    [[nodiscard]]
    DirBase resolve_dirfd_base(int dirfd) {
        if (dirfd == AT_FDCWD) {
            if (!ensure_cwd_fd_bound()) {
                return {};
            }
            dirfd = CWD_FD;
        }

        auto *state = find_dir_fd_state(dirfd);
        if (state == nullptr || state->abs_path == nullptr) {
            return {};
        }
        CapIdx cap = fd_to_cap(dirfd);
        if (cap == cap::error) {
            return {};
        }
        return DirBase{
            .cap      = cap,
            .abs_path = *state->abs_path,
        };
    }

    [[nodiscard]]
    ResolvedPath resolve_path_at(int dirfd, const char *pathname) {
        if (pathname == nullptr || pathname[0] == '\0') {
            return {};
        }

        auto path = normalize_path(make_path(pathname));
        if (!valid_normalized_path(path)) {
            return {};
        }

        if (path.is_absolute()) {
            auto absolute_path = path_to_string(path);
            return ResolvedPath{
                .parent_cap     = __prog_root_dir_cap,
                .absolute_path  = absolute_path,
                .relative_path  = absolute_path_to_relpath(path),
            };
        }

        auto base = resolve_dirfd_base(dirfd);
        if (base.cap == cap::null || base.cap == cap::error) {
            return {};
        }

        auto base_path = normalize_path(util::Path::from(base.abs_path));
        if (!base_path.is_absolute()) {
            return {};
        }

        auto absolute_path = normalize_path(base_path / path);
        if (!valid_normalized_path(absolute_path) || !absolute_path.is_absolute()) {
            return {};
        }

        return ResolvedPath{
            .parent_cap    = base.cap,
            .absolute_path = path_to_string(absolute_path),
            .relative_path = path_to_string(path),
        };
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
    bool refresh_cwd_dir_cap(const std::string &cwd_path) {
        auto cwd = normalize_path(util::Path::from(cwd_path));
        if (!cwd.is_absolute()) {
            return false;
        }
        auto relpath = absolute_path_to_relpath(cwd);
        if (relpath.empty()) {
            return false;
        }

        if (__prog_cwd_dir_cap != cap::null && __prog_cwd_dir_cap != cap::error) {
            sys_cap_remove(__prog_cwd_dir_cap);
            __prog_cwd_dir_cap = cap::null;
        }
        __prog_cwd = path_to_string(cwd);

        CapIdx cwd_cap = open_dir_cap_at(__prog_root_dir_cap, relpath);
        if (cwd_cap == cap::null || cwd_cap == cap::error) {
            return false;
        }

        __prog_cwd_dir_cap = cwd_cap;
        return ensure_cwd_fd_bound();
    }

    [[nodiscard]]
    ResolvedNodeType stat_resolved_path(const ResolvedPath &resolved,
                                        NodeMeta &meta) {
        if (resolved.parent_cap == cap::null || resolved.parent_cap == cap::error ||
            resolved.relative_path.empty())
        {
            return ResolvedNodeType::ERROR;
        }
        if (!sys_vfs_stat(resolved.parent_cap, resolved.relative_path.c_str(),
                          &meta))
        {
            return ResolvedNodeType::MISSING;
        }
        switch (meta.type) {
            case EntryType::DIR:  return ResolvedNodeType::DIRECTORY;
            case EntryType::FILE:
            case EntryType::SYMLINK:
                return ResolvedNodeType::FILE;
            default: return ResolvedNodeType::ERROR;
        }
    }

    void copy_dir_fd_state(int oldfd, int newfd, bool pinned) {
        auto *old_state = find_dir_fd_state(oldfd);
        if (old_state == nullptr || old_state->abs_path == nullptr) {
            clear_dir_fd_state(newfd);
            return;
        }

        auto register_res =
            register_dir_fd_state(newfd, *old_state->abs_path, pinned);
        if (register_res == static_cast<size_t>(-1)) {
            return;
        }
        auto *new_state = find_dir_fd_state(newfd);
        if (new_state != nullptr) {
            new_state->next_index = old_state->next_index;
            new_state->pinned     = pinned;
        }
    }

    size_t bind_open_result(int fd, CapIdx cap, size_t offset,
                            const std::string *dir_path, bool pinned) {
        if (fd < 0 || fd >= MAX_FDS) {
            if (cap != cap::null && cap != cap::error) {
                sys_cap_remove(cap);
            }
            return -EBADF;
        }

        clear_dir_fd_state(fd);
        if (!bind_fd(fd, cap)) {
            if (cap != cap::null && cap != cap::error) {
                sys_cap_remove(cap);
            }
            return -EBADF;
        }
        set_fd_offset(fd, offset);
        if (dir_path != nullptr &&
            register_dir_fd_state(fd, *dir_path, pinned) ==
                static_cast<size_t>(-1))
        {
            free_fd(fd);
            return -EMFILE;
        }
        return static_cast<size_t>(fd);
    }

    size_t do_open_resolved(const ResolvedPath &resolved, int flags) {
        if (resolved.parent_cap == cap::null || resolved.parent_cap == cap::error ||
            resolved.relative_path.empty())
        {
            return -ENOENT;
        }

        flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);
        bool want_directory          = (flags & LINUX_O_DIRECTORY) != 0;
        NodeMeta meta{};
        auto node_type               = stat_resolved_path(resolved, meta);
        if (node_type == ResolvedNodeType::ERROR) {
            return -EIO;
        }
        if (node_type == ResolvedNodeType::DIRECTORY) {
            want_directory = true;
        } else if (node_type == ResolvedNodeType::MISSING &&
                   (flags & LINUX_O_CREAT) == 0)
        {
            return -ENOENT;
        }

        CapIdx file_cap = want_directory
                              ? sys_vfs_opendir(resolved.parent_cap,
                                                resolved.relative_path.c_str(),
                                                sustcore_flags)
                              : sys_vfs_open(resolved.parent_cap,
                                             resolved.relative_path.c_str(),
                                             sustcore_flags);
        if (file_cap == cap::null || file_cap == cap::error) {
            loggers::LXSC::ERROR("Invalid path: %s", resolved.absolute_path.c_str());
            return want_directory ? -ENOTDIR : -ENOENT;
        }

        if (want_directory && register_dir_fd_state(CWD_FD, resolved.absolute_path, true),
            false)
        {
        }
        int fd = alloc_fd(file_cap);
        if (fd < 0) {
            sys_cap_remove(file_cap);
            return -EMFILE;
        }
        if (want_directory &&
            register_dir_fd_state(fd, resolved.absolute_path, false) ==
                static_cast<size_t>(-1))
        {
            free_fd(fd);
            return -EMFILE;
        }
        return static_cast<size_t>(fd);
    }
}  // namespace

size_t linux_open_fd(const char *pathname, int fd, int flags) {
    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);
    CapIdx file_cap =
        sys_vfs_open(resolved.parent_cap, resolved.relative_path.c_str(),
                     sustcore_flags);
    if (file_cap == cap::null || file_cap == cap::error) {
        return -ENOENT;
    }

    return bind_open_result(fd, file_cap, 0, nullptr, false);
}

size_t linux_opendir_fd(const char *pathname, int fd) {
    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    CapIdx dir_cap =
        sys_vfs_opendir(resolved.parent_cap, resolved.relative_path.c_str(),
                        flags::O_READ);
    if (dir_cap == cap::null || dir_cap == cap::error) {
        return -ENOTDIR;
    }

    return bind_open_result(fd, dir_cap, 0, &resolved.absolute_path,
                            fd == CWD_FD);
}

size_t linux_sys_write(size_t fd, const void *buf, size_t len) {
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
    if (fd == CWD_FD) {
        return -EBADF;
    }
    CapIdx cap = fd_to_cap(fd);
    if (cap == cap::error) {
        return -EBADF;
    }
    clear_dir_fd_state(fd);
    free_fd(fd);
    return 0;
}

size_t linux_sys_dup(int oldfd) {
    if (oldfd < 0) {
        return -EBADF;
    }

    CapIdx old_cap = fd_to_cap(oldfd);
    if (old_cap == cap::error) {
        return -EBADF;
    }

    CapIdx new_cap = sys_cap_clone(old_cap);
    if (new_cap == cap::null || new_cap == cap::error) {
        return -EBADF;
    }

    int newfd = alloc_fd(new_cap);
    if (newfd < 0) {
        sys_cap_remove(new_cap);
        return -EMFILE;
    }

    set_fd_offset(newfd, fd_offset(oldfd));
    copy_dir_fd_state(oldfd, newfd, false);
    return static_cast<size_t>(newfd);
}

size_t linux_sys_dup3(int oldfd, int newfd, int flags) {
    if (flags != 0) {
        return -EINVAL;
    }
    if (oldfd < 0 || newfd < 0) {
        return -EBADF;
    }
    if (oldfd == newfd) {
        return -EINVAL;
    }
    if (newfd >= MAX_FDS) {
        return -EBADF;
    }
    if (newfd == CWD_FD) {
        return -EBADF;
    }

    CapIdx old_cap = fd_to_cap(oldfd);
    if (old_cap == cap::error) {
        return -EBADF;
    }

    CapIdx new_cap = sys_cap_clone(old_cap);
    if (new_cap == cap::null || new_cap == cap::error) {
        return -EBADF;
    }

    clear_dir_fd_state(newfd);
    if (!bind_fd(newfd, new_cap)) {
        sys_cap_remove(new_cap);
        return -EBADF;
    }

    set_fd_offset(newfd, fd_offset(oldfd));
    copy_dir_fd_state(oldfd, newfd, false);
    return static_cast<size_t>(newfd);
}

size_t linux_sys_openat(int dirfd, const char *pathname, int flags, int mode) {
    (void)mode;

    if (pathname == nullptr) {
        return -EINVAL;
    }

    auto resolved = resolve_path_at(dirfd, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }
    return do_open_resolved(resolved, flags);
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

    size_t cwd_len = __prog_cwd.size() + 1;
    if (size < cwd_len) {
        return -ERANGE;
    }

    memcpy(buf, __prog_cwd.c_str(), cwd_len);
    return reinterpret_cast<size_t>(buf);
}

size_t linux_sys_chdir(const char *pathname) {
    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    CapIdx dir_cap =
        sys_vfs_opendir(resolved.parent_cap, resolved.relative_path.c_str(),
                        flags::O_READ);
    if (dir_cap == cap::null || dir_cap == cap::error) {
        return -ENOTDIR;
    }
    sys_cap_remove(dir_cap);

    return refresh_cwd_dir_cap(resolved.absolute_path) ? 0 : -EIO;
}

size_t linux_sys_mkdirat(int dirfd, const char *pathname, int mode) {
    (void)mode;

    if (pathname == nullptr) {
        return -EINVAL;
    }

    auto resolved = resolve_path_at(dirfd, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    CapIdx dir_cap =
        sys_vfs_mkdir(resolved.parent_cap, resolved.relative_path.c_str(),
                      flags::O_READ);
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
    if ((flags & ~AT_REMOVEDIR) != 0) {
        return -EINVAL;
    }

    auto resolved = resolve_path_at(dirfd, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    bool ok = (flags & AT_REMOVEDIR) != 0
                  ? sys_vfs_rmdir(resolved.parent_cap,
                                  resolved.relative_path.c_str())
                  : sys_vfs_unlink(resolved.parent_cap,
                                   resolved.relative_path.c_str());
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
