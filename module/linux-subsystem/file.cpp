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

#include <errno.h>
#include <fdtable.h>
#include <file.h>
#include <logger.h>
#include <pipe.h>
#include <prog.h>
#include <sustcore/bootstrap.h>
#include <sustcore/capability.h>
#include <sus/path.h>
#include <sustcore/files.h>
#include <syscall.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {
    constexpr size_t INVALID_VALUE  = 0xFFFF'FFFF'FFFF'FFFF;
    constexpr int AT_FDCWD          = -100;
    constexpr int AT_SYMLINK_NOFOLLOW = 0x100;
    constexpr int AT_EMPTY_PATH     = 0x1000;
    constexpr int LINUX_O_RDONLY    = 0;
    constexpr int LINUX_O_WRONLY    = 1;
    constexpr int LINUX_O_RDWR      = 2;
    constexpr int LINUX_O_CREAT     = 0100;     // octal
    constexpr int LINUX_O_DIRECTORY = 0200000;  // octal
    constexpr int AT_REMOVEDIR      = 0x200;
    constexpr size_t MAX_DIR_FDS    = 128;
    constexpr size_t MAX_EXEC_ARGS   = 256;
    constexpr uint8_t DT_REG        = 8;
    constexpr uint8_t DT_DIR        = 4;
    constexpr uint8_t DT_LNK        = 10;
    constexpr uint8_t DT_UNKNOWN    = 0;
    constexpr int LINUX_IOV_MAX     = 1024;

    struct linux_iovec {
        void *iov_base;
        size_t iov_len;
    };

    struct linux_dirent64 {
        uint64_t d_ino;
        int64_t d_off;
        unsigned short d_reclen;
        unsigned char d_type;
        char d_name[];
    };

    struct linux_statx_timestamp {
        int64_t tv_sec;
        uint32_t tv_nsec;
        int32_t __reserved;
    };

    struct linux_statx {
        uint32_t stx_mask;
        uint32_t stx_blksize;
        uint64_t stx_attributes;
        uint32_t stx_nlink;
        uint32_t stx_uid;
        uint32_t stx_gid;
        uint16_t stx_mode;
        uint16_t __spare0[1];
        uint64_t stx_ino;
        uint64_t stx_size;
        uint64_t stx_blocks;
        uint64_t stx_attributes_mask;
        linux_statx_timestamp stx_atime;
        linux_statx_timestamp stx_btime;
        linux_statx_timestamp stx_ctime;
        linux_statx_timestamp stx_mtime;
        uint32_t stx_rdev_major;
        uint32_t stx_rdev_minor;
        uint32_t stx_dev_major;
        uint32_t stx_dev_minor;
        uint64_t __spare2[14];
    };

    struct linux_kstat {
        uint64_t st_dev;
        uint64_t st_ino;
        uint32_t st_mode;
        uint32_t st_nlink;
        uint32_t st_uid;
        uint32_t st_gid;
        uint64_t st_rdev;
        unsigned long __pad;
        long st_size;
        uint32_t st_blksize;
        int __pad2;
        uint64_t st_blocks;
        long st_atime_sec;
        long st_atime_nsec;
        long st_mtime_sec;
        long st_mtime_nsec;
        long st_ctime_sec;
        long st_ctime_nsec;
        unsigned __unused[2];
    };

    constexpr uint32_t STATX_TYPE   = 0x0001U;
    constexpr uint32_t STATX_MODE   = 0x0002U;
    constexpr uint32_t STATX_NLINK  = 0x0004U;
    constexpr uint32_t STATX_UID    = 0x0008U;
    constexpr uint32_t STATX_GID    = 0x0010U;
    constexpr uint32_t STATX_ATIME  = 0x0020U;
    constexpr uint32_t STATX_INO    = 0x0100U;
    constexpr uint32_t STATX_SIZE   = 0x0200U;
    constexpr uint32_t STATX_BLOCKS = 0x0400U;
    constexpr uint32_t STATX_MTIME  = 0x0040U;
    constexpr uint32_t STATX_CTIME  = 0x0080U;
    constexpr uint16_t S_DEFAULTL   = 0x01A4U;
    constexpr uint16_t S_IFREG      = 0x8000U;
    constexpr uint16_t S_IFDIR      = 0x4000U;
    constexpr uint16_t S_IFLNK      = 0xA000U;
    // 占位符, 以后再引入真实时间
    constexpr int64_t FIXED_TIME    = 0x114514;

    struct DirFdState {
        bool used             = false;
        bool pinned           = false;
        int fd                = -1;
        size_t next_index     = 0;
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

    struct ReadlinkTarget {
        CapIdx parent_cap = cap::null;
        std::string relative_path{};
    };

    struct CapExplainBootstrap {
        bsheader header;
        BootstrapCapExplainPayloadHead explain;
        char desc[8];
    };

    struct PathBootstrap {
        bsheader header;
        char desc[LINUX_PATH_MAX + 5];
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
        state    = {};
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
    CapIdx open_dir_cap_at(CapIdx parent_cap,
                           const std::string &relative_path) {
        if (parent_cap == cap::null || parent_cap == cap::error) {
            return cap::error;
        }
        auto dir_res =
            sys_vfs_opendir(parent_cap, relative_path.c_str(), flags::O_READ)
                .to_result();
        return dir_res.has_value() ? dir_res.value() : cap::error;
    }

    [[nodiscard]]
    bool ensure_cwd_fd_bound() {
        if (__prog_cwd_dir_cap == cap::null || __prog_cwd_dir_cap == cap::error)
        {
            return false;
        }

        auto current_cap = fd_to_cap(CWD_FD);
        auto *entry      = lookup_fd(CWD_FD);
        auto *state      = find_dir_fd_state(CWD_FD);
        if (current_cap == __prog_cwd_dir_cap && entry != nullptr &&
            state != nullptr && state->abs_path != nullptr &&
            *state->abs_path == __prog_cwd)
        {
            entry->offset     = 0;
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
                .parent_cap    = __prog_root_dir_cap,
                .absolute_path = absolute_path,
                .relative_path = absolute_path_to_relpath(path),
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
        if (!valid_normalized_path(absolute_path) ||
            !absolute_path.is_absolute())
        {
            return {};
        }

        return ResolvedPath{
            .parent_cap    = base.cap,
            .absolute_path = path_to_string(absolute_path),
            .relative_path = path_to_string(path),
        };
    }

    [[nodiscard]]
    ReadlinkTarget resolve_empty_path_readlink_target(int dirfd) {
        if (dirfd == AT_FDCWD) {
            dirfd = CWD_FD;
        }

        auto *state = find_dir_fd_state(dirfd);
        if (state == nullptr || state->abs_path == nullptr) {
            return {};
        }

        auto abs_path = normalize_path(util::Path::from(*state->abs_path));
        if (!abs_path.is_absolute()) {
            return {};
        }

        auto text = path_to_string(abs_path);
        if (text == "/") {
            return {};
        }

        auto slash_pos = text.find_last_of('/');
        if (slash_pos == std::string::npos) {
            return {};
        }

        std::string rel_path = absolute_path_to_relpath(abs_path);
        if (rel_path.empty() || rel_path == ".") {
            return {};
        }

        if (slash_pos == 0) {
            return ReadlinkTarget{
                .parent_cap    = __prog_root_dir_cap,
                .relative_path = rel_path,
            };
        }

        auto parent = resolve_path_at(AT_FDCWD, text.substr(0, slash_pos).c_str());
        if (parent.parent_cap == cap::null || parent.parent_cap == cap::error) {
            return {};
        }

        return ReadlinkTarget{
            .parent_cap    = parent.parent_cap,
            .relative_path = text.substr(slash_pos + 1),
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
        size_t size     = sizeof(linux_dirent64) + name_len;
        size_t align    = alignof(uint64_t);
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

        auto *entry =
            reinterpret_cast<linux_dirent64 *>(static_cast<char *>(buf) + pos);
        entry->d_ino    = meta.inode;
        entry->d_off    = static_cast<int64_t>(next_index);
        entry->d_reclen = static_cast<unsigned short>(reclen);
        entry->d_type   = entry_type_to_dtype(meta.type);
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

        if (__prog_cwd_dir_cap != cap::null && __prog_cwd_dir_cap != cap::error)
        {
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
        if (resolved.parent_cap == cap::null ||
            resolved.parent_cap == cap::error || resolved.relative_path.empty())
        {
            return ResolvedNodeType::ERROR;
        }
        Result<void> stat_res =
            sys_vfs_stat(resolved.parent_cap, resolved.relative_path.c_str(),
                         &meta)
                .to_result();
        if (stat_res.has_value()) {
            switch (meta.type) {
                case EntryType::DIR:     return ResolvedNodeType::DIRECTORY;
                case EntryType::FILE:
                case EntryType::SYMLINK: return ResolvedNodeType::FILE;
                default:                 return ResolvedNodeType::ERROR;
            }
        }
        ErrCode err = stat_res.error();
        if (err == ErrCode::ENTRY_NOT_FOUND) {
            return ResolvedNodeType::MISSING;
        }
        return ResolvedNodeType::ERROR;
    }

    [[nodiscard]]
    uint16_t node_meta_to_mode(const NodeMeta &meta) {
        switch (meta.type) {
            case EntryType::DIR:
                return static_cast<uint16_t>(S_IFDIR | S_DEFAULTL);
            case EntryType::SYMLINK:
                return static_cast<uint16_t>(S_IFLNK | S_DEFAULTL);
            case EntryType::FILE:
            default:              return static_cast<uint16_t>(S_IFREG | S_DEFAULTL);
        }
    }

    void fill_fixed_time(linux_statx_timestamp &ts) {
        ts.tv_sec     = FIXED_TIME;
        ts.tv_nsec    = 0;
        ts.__reserved = 0;
    }

    void fill_statx_from_node_meta(const NodeMeta &meta, linux_statx &stx) {
        memset(&stx, 0, sizeof(stx));
        stx.stx_mask = STATX_TYPE | STATX_MODE | STATX_NLINK | STATX_UID |
                       STATX_GID | STATX_ATIME | STATX_INO | STATX_SIZE |
                       STATX_BLOCKS | STATX_MTIME | STATX_CTIME;
        stx.stx_blksize = 4096;
        stx.stx_nlink   = static_cast<uint32_t>(meta.links);
        stx.stx_mode    = node_meta_to_mode(meta);
        stx.stx_ino     = meta.inode;
        stx.stx_size    = meta.size;
        stx.stx_blocks  = (meta.size + 511) / 512;
        fill_fixed_time(stx.stx_atime);
        fill_fixed_time(stx.stx_btime);
        fill_fixed_time(stx.stx_ctime);
        fill_fixed_time(stx.stx_mtime);
    }

    void fill_kstat_from_node_meta(const NodeMeta &meta, linux_kstat &kst) {
        memset(&kst, 0, sizeof(kst));
        kst.st_ino        = meta.inode;
        kst.st_mode       = node_meta_to_mode(meta);
        kst.st_nlink      = static_cast<uint32_t>(meta.links);
        kst.st_size       = static_cast<long>(meta.size);
        kst.st_blksize    = 4096;
        kst.st_blocks     = (meta.size + 511) / 512;
        kst.st_atime_sec  = FIXED_TIME;
        kst.st_mtime_sec  = FIXED_TIME;
        kst.st_ctime_sec  = FIXED_TIME;
    }

    [[nodiscard]]
    size_t stat_path_at(int dirfd, const char *pathname, bool nofollow,
                        NodeMeta &meta) {
        auto resolved = resolve_path_at(dirfd, pathname);
        if (resolved.parent_cap == cap::null || resolved.parent_cap == cap::error ||
            resolved.relative_path.empty())
        {
            return -ENOENT;
        }

        auto stat_res =
            (nofollow ? sys_vfs_lstat(resolved.parent_cap,
                                      resolved.relative_path.c_str(), &meta)
                      : sys_vfs_stat(resolved.parent_cap,
                                     resolved.relative_path.c_str(), &meta))
                .to_result();
        if (!stat_res.has_value()) {
            return stat_res.error() == ErrCode::ENTRY_NOT_FOUND ? -ENOENT : -EIO;
        }
        return 0;
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

    [[nodiscard]]
    ResolvedNodeType create_resolved_node(const ResolvedPath &resolved,
                                          flags::oflg_t sustcore_flags,
                                          bool flg_directory,
                                          CapIdx &file_cap) {
        file_cap = cap::error;
        if (flg_directory) {
            auto dir_res =
                sys_vfs_mkdir(resolved.parent_cap, resolved.relative_path.c_str(),
                              sustcore_flags)
                    .to_result();
            file_cap = dir_res.has_value() ? dir_res.value() : cap::error;
            return ResolvedNodeType::DIRECTORY;
        }

        auto file_res =
            sys_vfs_mkfile(resolved.parent_cap, resolved.relative_path.c_str(),
                           sustcore_flags)
                .to_result();
        file_cap = file_res.has_value() ? file_res.value() : cap::error;
        return ResolvedNodeType::FILE;
    }

    size_t do_open_resolved(const ResolvedPath &resolved, int flags) {
        if (!cap::valid(resolved.parent_cap) || resolved.relative_path.empty())
        {
            return -ENOENT;
        }

        bool flg_create    = (flags & LINUX_O_CREAT) != 0;
        bool flg_directory = (flags & LINUX_O_DIRECTORY) != 0;

        flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);
        NodeMeta meta{};
        auto node_type               = stat_resolved_path(resolved, meta);
        CapIdx file_cap              = cap::error;

        if (node_type == ResolvedNodeType::ERROR) {
            return -EIO;
        } else if (node_type == ResolvedNodeType::MISSING) {
            if (!flg_create) {
                loggers::LXSC::ERROR("openat path=%s does not exist and O_CREAT not set",
                                 resolved.absolute_path.c_str());
                return -ENOENT;
            }
            loggers::LXSC::INFO("openat create path=%s type=%s",
                                resolved.absolute_path.c_str(),
                                flg_directory ? "directory" : "file");
            node_type =
                create_resolved_node(resolved, sustcore_flags, flg_directory,
                                     file_cap);
        } else if (node_type == ResolvedNodeType::DIRECTORY) {
            auto dir_res =
                sys_vfs_opendir(resolved.parent_cap,
                                resolved.relative_path.c_str(), sustcore_flags)
                    .to_result();
            file_cap = dir_res.has_value() ? dir_res.value() : cap::error;
        } else if (node_type == ResolvedNodeType::FILE) {
            if (flg_directory) {
                return -ENOTDIR;
            }
            auto file_res =
                sys_vfs_open(resolved.parent_cap,
                             resolved.relative_path.c_str(), sustcore_flags)
                    .to_result();
            file_cap = file_res.has_value() ? file_res.value() : cap::error;
        }

        if (!cap::valid(file_cap)) {
            loggers::LXSC::ERROR("invalid open result path=%s type=%s",
                                 resolved.absolute_path.c_str(),
                                 node_type == ResolvedNodeType::DIRECTORY
                                     ? "directory"
                                     : "file");
            return node_type == ResolvedNodeType::DIRECTORY ? -ENOTDIR
                                                            : -ENOENT;
        }

        int fd = alloc_fd(file_cap);
        if (fd < 0) {
            sys_cap_remove(file_cap);
            return -EMFILE;
        }

        if (node_type == ResolvedNodeType::DIRECTORY &&
            register_dir_fd_state(fd, resolved.absolute_path, false) ==
                static_cast<size_t>(-1))
        {
            free_fd(fd);
            return -EMFILE;
        }
        return static_cast<size_t>(fd);
    }

    [[nodiscard]]
    bool copy_exec_strings(const char *const src[], std::vector<std::string> &dst) {
        dst.clear();
        if (src == nullptr) {
            return true;
        }

        for (size_t i = 0; src[i] != nullptr; ++i) {
            if (i >= MAX_EXEC_ARGS || strlen(src[i]) >= LINUX_PATH_MAX) {
                return false;
            }
            dst.emplace_back(src[i]);
        }
        return true;
    }

    void make_exec_ptrs(const std::vector<std::string> &items,
                        std::vector<const char *> &ptrs) {
        ptrs.clear();
        for (const auto &item : items) {
            ptrs.push_back(item.c_str());
        }
        ptrs.push_back(nullptr);
    }

    void fill_cap_bootstrap(CapExplainBootstrap &record, CapIdx cap_idx,
                            PayloadType type, b64 perm, const char *desc) {
        memset(&record, 0, sizeof(record));
        record.header.size = sizeof(bsheader) +
                             sizeof(BootstrapCapExplainPayloadHead) +
                             strlen(desc) + 1;
        record.header.type      = boot::TYPE_CAPEXP;
        record.explain.cap_idx  = cap_idx;
        record.explain.cap_type = type;
        record.explain.cap_perm = perm;
        strcpy(record.desc, desc);
    }

    [[nodiscard]]
    bool fill_cwd_path_bootstrap(PathBootstrap &record) {
        memset(&record, 0, sizeof(record));
        int written = snprintf(record.desc, sizeof(record.desc), "#cwd:%s",
                               __prog_cwd.c_str());
        if (written <= 0 ||
            static_cast<size_t>(written) >= sizeof(record.desc)) {
            return false;
        }
        record.header.size = sizeof(bsheader) + static_cast<size_t>(written) + 1;
        record.header.type = boot::TYPE_PATHEXP;
        return true;
    }

    size_t fail_execve_after_open(int image_fd, size_t err) {
        linux_sys_close(image_fd);
        return err;
    }
}  // namespace

size_t linux_open_fd(const char *pathname, int fd, int flags) {
    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);
    auto file_res                = sys_vfs_open(resolved.parent_cap,
                                                resolved.relative_path.c_str(), sustcore_flags)
                        .to_result();
    CapIdx file_cap = file_res.has_value() ? file_res.value() : cap::error;
    if (file_cap == cap::null || file_cap == cap::error) {
        return -ENOENT;
    }

    return bind_open_result(fd, file_cap, 0, nullptr, false);
}

size_t linux_bind_cap_fd(CapIdx cap, int fd, bool append) {
    if (cap == cap::null || cap == cap::error) {
        return -EBADF;
    }

    size_t offset = 0;
    if (append) {
        auto size_res = sys_vfs_size(cap).to_result();
        if (!size_res.has_value()) {
            return -EIO;
        }
        offset = size_res.value();
    }

    auto clone_res = sys_cap_clone(cap).to_result();
    if (!clone_res.has_value()) {
        return -EBADF;
    }
    return bind_open_result(fd, clone_res.value(), offset, nullptr, false);
}

size_t linux_opendir_fd(const char *pathname, int fd) {
    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    auto dir_res =
        sys_vfs_opendir(resolved.parent_cap, resolved.relative_path.c_str(),
                        flags::O_READ)
            .to_result();
    CapIdx dir_cap = dir_res.has_value() ? dir_res.value() : cap::error;
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
    if (! cap::valid(file_cap)) {
        printf("linux_sys_write: invalid fd %u, parsed to invalid cap idx\n", fd);
        return -EBADF;
    }
    if (linux_cap_is_pipe_write_end(file_cap)) {
        return linux_pipe_write(file_cap, buf, len);
    }
    if (linux_cap_is_pipe_read_end(file_cap)) {
        return -EBADF;
    }

    size_t offset  = fd_offset(static_cast<int>(fd));
    auto write_res = sys_vfs_write(file_cap, offset,
                                   reinterpret_cast<const void *>(buf), len)
                         .to_result();
    if (!write_res.has_value()) {
        return -EIO;
    }
    size_t written = write_res.value();

    set_fd_offset(static_cast<int>(fd), offset + written);
    return written;
}

size_t linux_sys_writev(int fd, const void *iov, int iovcnt) {
    if (iovcnt < 0 || iovcnt > LINUX_IOV_MAX) {
        return -EINVAL;
    }
    if (iovcnt == 0) {
        return 0;
    }
    if (iov == nullptr) {
        return -EFAULT;
    }

    const auto *iovecs = static_cast<const linux_iovec *>(iov);
    size_t total       = 0;
    for (int i = 0; i < iovcnt; ++i) {
        const auto &entry = iovecs[i];
        if (entry.iov_base == nullptr && entry.iov_len != 0) {
            return total != 0 ? total : static_cast<size_t>(-EFAULT);
        }
        size_t wrote = linux_sys_write(
            static_cast<size_t>(fd), entry.iov_base, entry.iov_len);
        if (static_cast<long>(wrote) < 0) {
            return total != 0 ? total : wrote;
        }
        total += wrote;
        if (wrote != entry.iov_len) {
            break;
        }
    }
    return total;
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
    if (linux_cap_is_pipe_read_end(file_cap)) {
        return linux_pipe_read(file_cap, buf, count);
    }
    if (linux_cap_is_pipe_write_end(file_cap)) {
        return -EBADF;
    }

    size_t offset = fd_offset(fd);
    auto read_res = sys_vfs_read(file_cap, offset, buf, count).to_result();
    if (!read_res.has_value()) {
        return -EIO;
    }
    size_t nread = read_res.value();

    set_fd_offset(fd, offset + nread);
    return nread;
}

size_t linux_sys_readv(int fd, const void *iov, int iovcnt) {
    if (iovcnt < 0 || iovcnt > LINUX_IOV_MAX) {
        return -EINVAL;
    }
    if (iovcnt == 0) {
        return 0;
    }
    if (iov == nullptr) {
        return -EFAULT;
    }

    const auto *iovecs = static_cast<const linux_iovec *>(iov);
    size_t total       = 0;
    for (int i = 0; i < iovcnt; ++i) {
        const auto &entry = iovecs[i];
        if (entry.iov_base == nullptr && entry.iov_len != 0) {
            return total != 0 ? total : static_cast<size_t>(-EFAULT);
        }
        size_t nread = linux_sys_read(fd, entry.iov_base, entry.iov_len);
        if (static_cast<long>(nread) < 0) {
            return total != 0 ? total : nread;
        }
        total += nread;
        if (nread != entry.iov_len) {
            break;
        }
    }
    return total;
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

    auto new_cap_res = sys_cap_clone(old_cap).to_result();
    if (!new_cap_res.has_value()) {
        return -EBADF;
    }
    CapIdx new_cap = new_cap_res.value();

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

    auto new_cap_res = sys_cap_clone(old_cap).to_result();
    if (!new_cap_res.has_value()) {
        return -EBADF;
    }
    CapIdx new_cap = new_cap_res.value();

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
            default: return -EINVAL;
        }
    }

    CapIdx file_cap = fd_to_cap(fd);
    if (file_cap == cap::error) {
        return -EBADF;
    }

    size_t new_offset = 0;
    switch (whence) {
        case 0: new_offset = offset; break;
        case 1: new_offset = fd_offset(fd) + offset; break;
        case 2: {
            auto file_size_res = sys_vfs_size(file_cap).to_result();
            if (!file_size_res.has_value()) {
                return -EIO;
            }
            size_t file_size = file_size_res.value();
            new_offset       = file_size + offset;
            break;
        }
        default: return -EINVAL;
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

size_t linux_sys_readlinkat(int dirfd, const char *pathname, char *buf,
                            size_t bufsiz) {
    if (pathname == nullptr) {
        return -EINVAL;
    }
    if (buf == nullptr && bufsiz != 0) {
        return -EFAULT;
    }
    if (bufsiz == 0) {
        return 0;
    }

    CapIdx parent_cap           = cap::null;
    std::string relative_path{};
    if (pathname[0] == '\0') {
        auto target = resolve_empty_path_readlink_target(dirfd);
        parent_cap  = target.parent_cap;
        relative_path = std::move(target.relative_path);
    } else {
        auto resolved = resolve_path_at(dirfd, pathname);
        parent_cap = resolved.parent_cap;
        relative_path = std::move(resolved.relative_path);
    }

    if (parent_cap == cap::null || parent_cap == cap::error ||
        relative_path.empty())
    {
        return pathname[0] == '\0' ? -EBADF : -ENOENT;
    }

    auto readlink_res =
        sys_vfs_readlink(parent_cap, relative_path.c_str(), buf, bufsiz)
            .to_result();
    if (!readlink_res.has_value()) {
        if (readlink_res.error() == ErrCode::ENTRY_NOT_FOUND) {
            loggers::LXSC::ERROR("readlinkat failed: path not found dirfd=%d path=%s",
                                 dirfd, pathname);
            return -ENOENT;
        }
        return readlink_res.error() == ErrCode::ENTRY_NOT_FOUND ? -ENOENT
                                                                : -EIO;
    }
    return readlink_res.value();
}

size_t linux_sys_chdir(const char *pathname) {
    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    auto dir_res =
        sys_vfs_opendir(resolved.parent_cap, resolved.relative_path.c_str(),
                        flags::O_READ)
            .to_result();
    CapIdx dir_cap = dir_res.has_value() ? dir_res.value() : cap::error;
    if (dir_cap == cap::null || dir_cap == cap::error) {
        return -ENOTDIR;
    }
    (void)sys_cap_remove(dir_cap);

    return refresh_cwd_dir_cap(resolved.absolute_path) ? 0 : -EIO;
}

size_t linux_sys_execve(const char *pathname, const char *const argv[],
                        const char *const envp[]) {
    if (pathname == nullptr) {
        return -EFAULT;
    }

    auto resolved = resolve_path_at(AT_FDCWD, pathname);
    if (resolved.parent_cap == cap::null || resolved.relative_path.empty()) {
        return -ENOENT;
    }

    SysRet<CapIdx> image_cap_ret = sys_vfs_open(resolved.parent_cap,
        resolved.relative_path.c_str(),
        flags::O_EXECUTE | flags::O_READ);

    if (image_cap_ret.is_error()) {
        return -ENOENT;
    }

    CapIdx image_cap = image_cap_ret.value();

    int image_fd = alloc_fd(image_cap);
    if (image_fd < 0) {
        sys_cap_remove(image_cap);
        return -EMFILE;
    }

    CapIdx root_cap = __prog_root_dir_cap;
    if (root_cap == cap::null || root_cap == cap::error) {
        linux_sys_close(image_fd);
        return -EIO;
    }

    CapIdx cwd_cap = __prog_cwd_dir_cap;
    if (cwd_cap == cap::null || cwd_cap == cap::error) {
        linux_sys_close(image_fd);
        return -EIO;
    }

    CapIdx parent_cap = __prog_parent_cap;
    if (parent_cap == cap::error) {
        parent_cap = cap::null;
    }

    CapIdx reserved_caps[] = {root_cap, cwd_cap, parent_cap, cap::null};

    CapExplainBootstrap root_bootstrap{};
    CapExplainBootstrap cwd_bootstrap{};
    CapExplainBootstrap parent_bootstrap{};
    PathBootstrap cwd_path_bootstrap{};

    fill_cap_bootstrap(root_bootstrap, root_cap, PayloadType::VDIR, ~b64(0),
                       "#/");
    fill_cap_bootstrap(cwd_bootstrap, cwd_cap, PayloadType::VDIR, ~b64(0),
                       "#cwd");
    if (parent_cap != cap::null && parent_cap != cap::error) {
        fill_cap_bootstrap(parent_bootstrap, parent_cap, PayloadType::PCB,
                           ~b64(0), "#parent");
    }
    if (!fill_cwd_path_bootstrap(cwd_path_bootstrap)) {
        return fail_execve_after_open(image_fd, -ENAMETOOLONG);
    }

    std::vector<std::string> argv_storage{};
    std::vector<std::string> envp_storage{};
    if (!copy_exec_strings(argv, argv_storage) ||
        !copy_exec_strings(envp, envp_storage))
    {
        return fail_execve_after_open(image_fd, -E2BIG);
    }

    if (argv_storage.empty()) {
        argv_storage.emplace_back(pathname);
    }

    std::vector<const char *> argv_ptrs{};
    std::vector<const char *> envp_ptrs{};
    make_exec_ptrs(argv_storage, argv_ptrs);
    make_exec_ptrs(envp_storage, envp_ptrs);

    const char *bsargv_with_parent[] = {
        reinterpret_cast<const char *>(&root_bootstrap),
        reinterpret_cast<const char *>(&cwd_bootstrap),
        reinterpret_cast<const char *>(&parent_bootstrap),
        reinterpret_cast<const char *>(&cwd_path_bootstrap),
        nullptr,
    };
    const char *bsargv_without_parent[] = {
        reinterpret_cast<const char *>(&root_bootstrap),
        reinterpret_cast<const char *>(&cwd_bootstrap),
        reinterpret_cast<const char *>(&cwd_path_bootstrap),
        nullptr,
    };
    const char **bsargv = parent_cap != cap::null && parent_cap != cap::error
                              ? bsargv_with_parent
                              : bsargv_without_parent;

    CapIdx exec_cap = fd_to_cap(image_fd);
    if (exec_cap == cap::error) {
        return fail_execve_after_open(image_fd, -EBADF);
    }

    ExecveRequest request{
        .image_cap = exec_cap,
        .execfn    = pathname,
        .caps      = reserved_caps,
        .argv      = argv_ptrs.data(),
        .envp      = envp_ptrs.data(),
        .bsargv    = bsargv,
    };

    if (!sys_pcb_execve_linux(__prog_pcb_cap, &request)) {
        linux_sys_close(image_fd);
        return -EIO;
    }

    while (true) {
    }
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

    auto dir_res = sys_vfs_mkdir(resolved.parent_cap,
                                 resolved.relative_path.c_str(), flags::O_READ)
                       .to_result();
    CapIdx dir_cap = dir_res.has_value() ? dir_res.value() : cap::error;
    if (dir_cap == cap::null || dir_cap == cap::error) {
        return -EIO;
    }
    (void)sys_cap_remove(dir_cap);
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

    bool ok = ((flags & AT_REMOVEDIR) != 0
                   ? sys_vfs_rmdir(resolved.parent_cap,
                                   resolved.relative_path.c_str())
                   : sys_vfs_unlink(resolved.parent_cap,
                                    resolved.relative_path.c_str()));
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

    auto *dir_state   = find_dir_fd_state(fd);
    size_t next_index = 0;
    if (dir_state != nullptr) {
        next_index = dir_state->next_index;
    }

    char raw_entries[LINUX_PATH_MAX]{};
    auto raw_size_res =
        sys_vfs_getdents(dir_cap, raw_entries, sizeof(raw_entries), next_index)
            .to_result();
    size_t raw_size =
        raw_size_res.has_value() ? raw_size_res.value() : INVALID_VALUE;
    if (raw_size == INVALID_VALUE) {
        return -EIO;
    }
    if (raw_size == 0) {
        return 0;
    }

    size_t pos         = 0;
    size_t raw_pos     = 0;
    size_t entry_index = next_index;
    while (raw_pos + sizeof(dir_entry_header) <= raw_size) {
        auto *header =
            reinterpret_cast<const dir_entry_header *>(raw_entries + raw_pos);
        if (header->next_offset == 0 ||
            raw_pos + header->next_offset > raw_size)
        {
            break;
        }

        const char *name = raw_entries + raw_pos + sizeof(dir_entry_header);
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

size_t linux_sys_statx(int dirfd, const char *pathname, int flags,
                       unsigned mask, void *statxbuf) {
    (void)mask;
    if (statxbuf == nullptr) {
        return -EINVAL;
    }
    if (dirfd < 0 || pathname == nullptr || pathname[0] != '\0' ||
        (flags & AT_EMPTY_PATH) == 0)
    {
        return -ENOSYS;
    }

    CapIdx cap = fd_to_cap(dirfd);
    if (cap == cap::error || cap == cap::null) {
        return -EBADF;
    }

    NodeMeta meta{};
    if (!sys_vfs_fstat(cap, &meta)) {
        return -EIO;
    }

    linux_statx stx{};
    fill_statx_from_node_meta(meta, stx);
    memcpy(statxbuf, &stx, sizeof(stx));
    return 0;
}

size_t linux_sys_fstat(int fd, void *statbuf) {
    if (statbuf == nullptr) {
        return -EINVAL;
    }

    CapIdx cap = fd_to_cap(fd);
    if (cap == cap::error || cap == cap::null) {
        return -EBADF;
    }

    NodeMeta meta{};
    if (!sys_vfs_fstat(cap, &meta)) {
        return -EIO;
    }

    linux_kstat kst{};
    fill_kstat_from_node_meta(meta, kst);
    memcpy(statbuf, &kst, sizeof(kst));
    return 0;
}

size_t linux_sys_newfstatat(int dirfd, const char *pathname, void *statbuf,
                            int flags) {
    constexpr int SUPPORTED_FLAGS = AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH;

    if (statbuf == nullptr || pathname == nullptr) {
        return -EINVAL;
    }
    if ((flags & ~SUPPORTED_FLAGS) != 0) {
        return -EINVAL;
    }

    NodeMeta meta{};
    if (pathname[0] == '\0') {
        if ((flags & AT_EMPTY_PATH) == 0) {
            return -ENOENT;
        }

        CapIdx cap = fd_to_cap(dirfd);
        if (cap == cap::error || cap == cap::null) {
            return -EBADF;
        }
        if (!sys_vfs_fstat(cap, &meta)) {
            return -EIO;
        }
    } else {
        size_t stat_ret =
            stat_path_at(dirfd, pathname, (flags & AT_SYMLINK_NOFOLLOW) != 0,
                         meta);
        if (static_cast<long>(stat_ret) < 0) {
            return stat_ret;
        }
    }

    linux_kstat kst{};
    fill_kstat_from_node_meta(meta, kst);
    memcpy(statbuf, &kst, sizeof(kst));
    return 0;
}
