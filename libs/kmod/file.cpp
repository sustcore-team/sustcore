/**
 * @file file.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kmod 文件描述符封装
 * @version alpha-1.0.0
 * @date 2026-06-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sustcore/bootstrap.h>
#include <kmod/syscall.h>
#include <prm.h>

#include <cstdio>
#include <cstring>

namespace {
    struct FileStructure {
        CapIdx cap    = cap::null;
        size_t offset = 0;
        bool used     = false;
    };

    struct DirBinding {
        CapIdx cap          = cap::null;
        const char *path    = nullptr;
        size_t path_len     = 0;
        bool from_bootstrap = false;
    };

    // allow up to 32 open files simultaneously,
    // which is temporarily sufficient for our needs
    constexpr int MAX_KMOD_FILES = 32;
    FileStructure g_files[MAX_KMOD_FILES]{};
    constexpr int MAX_DIR_BINDINGS = 16;
    DirBinding g_dir_bindings[MAX_DIR_BINDINGS]{};
    bool g_dir_bindings_loaded = false;

    // to find the free slot to insert the file
    [[nodiscard]]
    int alloc_fd() {
        for (int i = 0; i < MAX_KMOD_FILES; ++i) {
            if (!g_files[i].used) {
                g_files[i].used   = true;
                g_files[i].cap    = cap::null;
                g_files[i].offset = 0;
                return i;
            }
        }
        return -1;
    }

    struct OpenBase {
        CapIdx cap          = cap::null;
        const char *relpath = nullptr;
    };

    [[nodiscard]]
    bool binding_matches(const DirBinding &binding, const char *path) {
        if (binding.cap == cap::null || binding.path == nullptr ||
            binding.path_len == 0 || path == nullptr)
        {
            return false;
        }
        if (strncmp(path, binding.path, binding.path_len) != 0) {
            return false;
        }
        if (binding.path_len == 1) {
            return path[0] == '/';
        }
        return path[binding.path_len] == '\0' || path[binding.path_len] == '/';
    }

    void ensure_dir_bindings_loaded() {
        if (g_dir_bindings_loaded) {
            return;
        }
        g_dir_bindings_loaded = true;
        int binding_count     = 0;
        (void)bootstrap_foreach_record(
            __bsargv, __bsargc, [&](const BootstrapRecordView &view) {
                if (view.header->type != boot::TYPE_CAPEXP ||
                    binding_count >= MAX_DIR_BINDINGS)
                {
                    return;
                }
                BootstrapCapExplainView cap_explain{};
                if (!bootstrap_parse_cap_explain(view, cap_explain) ||
                    cap_explain.cap_type != PayloadType::VDIR ||
                    cap_explain.cap_idx == cap::null ||
                    cap_explain.cap_idx == cap::error ||
                    cap_explain.cap_desc == nullptr ||
                    cap_explain.cap_desc[0] != '#')
                {
                    return;
                }
                const char *path = cap_explain.cap_desc + 1;
                for (int i = 0; i < binding_count; ++i) {
                    if (strcmp(g_dir_bindings[i].path, path) == 0) {
                        return;
                    }
                }
                g_dir_bindings[binding_count++] = DirBinding{
                    .cap            = cap_explain.cap_idx,
                    .path           = path,
                    .path_len       = static_cast<size_t>(strlen(path)),
                    .from_bootstrap = true,
                };
            });
    }

    [[nodiscard]]
    OpenBase resolve_via_bindings(const char *path) {
        ensure_dir_bindings_loaded();
        const DirBinding *best = nullptr;
        for (const auto &binding : g_dir_bindings) {
            if (!binding_matches(binding, path)) {
                continue;
            }
            if (best == nullptr || binding.path_len > best->path_len) {
                best = &binding;
            }
        }
        if (best == nullptr) {
            return {};
        }

        const char *relpath = path + best->path_len;
        if (best->path_len == 1 && relpath[0] == '/') {
            ++relpath;
        }
        if (relpath[0] == '/') {
            ++relpath;
        }
        return OpenBase{.cap = best->cap, .relpath = relpath};
    }

    [[nodiscard]]
    OpenBase resolve_open_base(const char *path) {
        if (path == nullptr || path[0] != '/') {
            return {};
        }

        auto bootstrap_base = resolve_via_bindings(path);
        if (bootstrap_base.cap != cap::null &&
            bootstrap_base.relpath != nullptr)
        {
            return bootstrap_base;
        }
        return {};
    }

    [[nodiscard]]
    bool parse_open_options(const char *options, flags::oflg_t &oflags,
                            bool &append) {
        oflags = 0;
        append = false;
        if (options == nullptr) {
            return false;
        }
        if (strcmp(options, "r") == 0) {
            oflags = flags::O_READ;
            return true;
        }
        if (strcmp(options, "r+") == 0) {
            oflags = flags::O_READ | flags::O_WRITE;
            return true;
        }
        if (strcmp(options, "w") == 0) {
            oflags = flags::O_WRITE;
            return true;
        }
        if (strcmp(options, "w+") == 0) {
            oflags = flags::O_READ | flags::O_WRITE;
            return true;
        }
        if (strcmp(options, "a") == 0) {
            oflags = flags::O_WRITE;
            append = true;
            return true;
        }
        if (strcmp(options, "a+") == 0) {
            oflags = flags::O_READ | flags::O_WRITE;
            append = true;
            return true;
        }
        if (strcmp(options, "x") == 0) {
            oflags = flags::O_EXECUTE;
            return true;
        }
        return false;
    }

    // fd -> FileStructure
    [[nodiscard]]
    FileStructure *lookup_fd(int fd) {
        if (fd < 0 || fd >= MAX_KMOD_FILES || !g_files[fd].used) {
            return nullptr;
        }
        return &g_files[fd];
    }
}  // namespace

extern "C" {
int kmod_fopen(const char *path, const char *options) {
    flags::oflg_t oflags = 0;
    bool append          = false;
    auto base            = resolve_open_base(path);
    if (!parse_open_options(options, oflags, append) || base.cap == cap::null ||
        base.relpath == nullptr || *base.relpath == '\0')
    {
        return -1;
    }

    int fd = alloc_fd();
    if (fd < 0) {
        return -1;
    }

    CapIdx cap = sys_vfs_open(base.cap, base.relpath, oflags);
    if (cap == cap::error || cap == cap::null) {
        g_files[fd] = {};
        return -1;
    }

    g_files[fd].cap = cap;
    if (append) {
        g_files[fd].offset = sys_vfs_size(cap);
    }
    return fd;
}

int kmod_opendir(const char *path) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0')
    {
        return -1;
    }

    int fd = alloc_fd();
    if (fd < 0) {
        return -1;
    }

    CapIdx cap = sys_vfs_opendir(base.cap, base.relpath, flags::O_READ);
    if (cap == cap::error || cap == cap::null) {
        g_files[fd] = {};
        return -1;
    }

    g_files[fd].cap    = cap;
    g_files[fd].offset = 0;
    return fd;
}

size_t kmod_fread(int fd, void *buf, size_t len) {
    auto *file = lookup_fd(fd);
    if (file == nullptr || buf == nullptr) {
        return 0;
    }
    size_t got    = sys_vfs_read(file->cap, file->offset, buf, len);
    file->offset += got;
    return got;
}

size_t kmod_fwrite(int fd, const void *buf, size_t len) {
    auto *file = lookup_fd(fd);
    if (file == nullptr || buf == nullptr) {
        return 0;
    }
    size_t written  = sys_vfs_write(file->cap, file->offset, buf, len);
    file->offset   += written;
    return written;
}

CapIdx kmod_getcap(int fd) {
    auto *file = lookup_fd(fd);
    if (file == nullptr) {
        return cap::error;
    }
    return file->cap;
}

int kmod_mkdir(const char *path) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0')
    {
        return -1;
    }

    CapIdx cap =
        sys_vfs_mkdir(base.cap, base.relpath,
                      flags::O_READ | flags::O_WRITE | flags::O_EXECUTE);
    if (cap == cap::error || cap == cap::null) {
        return -1;
    }
    sys_cap_remove(cap);
    return 0;
}

int kmod_unlink(const char *path) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0')
    {
        return -1;
    }
    return sys_vfs_unlink(base.cap, base.relpath) ? 0 : -1;
}

int kmod_rmdir(const char *path) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0')
    {
        return -1;
    }
    return sys_vfs_rmdir(base.cap, base.relpath) ? 0 : -1;
}

int kmod_truncate(const char *path, size_t new_size) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0')
    {
        return -1;
    }
    CapIdx cap = sys_vfs_open(base.cap, base.relpath, flags::O_WRITE);
    if (cap == cap::error || cap == cap::null)
        return -1;
    bool ok = sys_vfs_truncate(cap, new_size);
    sys_cap_remove(cap);
    return ok ? 0 : -1;
}

int kmod_rename(const char *old_path, const char *new_path) {
    auto old_base = resolve_open_base(old_path);
    auto new_base = resolve_open_base(new_path);
    if (old_base.cap == cap::null || old_base.relpath == nullptr ||
        *old_base.relpath == '\0' || new_base.cap == cap::null ||
        new_base.relpath == nullptr || *new_base.relpath == '\0')
    {
        return -1;
    }
    return sys_vfs_rename(old_base.cap, old_base.relpath, new_base.cap,
                          new_base.relpath)
               ? 0
               : -1;
}

int kmod_symlink(const char *path, const char *target) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0' || target == nullptr || target[0] == '\0')
    {
        return -1;
    }
    return sys_vfs_symlink(base.cap, base.relpath, target) ? 0 : -1;
}

int kmod_link(const char *path, const char *target_path) {
    auto base = resolve_open_base(path);
    if (base.cap == cap::null || base.relpath == nullptr ||
        *base.relpath == '\0')
    {
        return -1;
    }
    auto target_base = resolve_open_base(target_path);
    if (target_base.cap == cap::null || target_base.relpath == nullptr ||
        *target_base.relpath == '\0')
    {
        return -1;
    }
    CapIdx target =
        sys_vfs_open(target_base.cap, target_base.relpath, flags::O_READ);
    if (target == cap::error || target == cap::null)
        return -1;
    bool ok = sys_vfs_link(base.cap, base.relpath, target);
    sys_cap_remove(target);
    return ok ? 0 : -1;
}

int kmod_mkfile(const char *path, const char *options) {
    flags::oflg_t oflags = 0;
    bool append          = false;
    auto base            = resolve_open_base(path);
    if (!parse_open_options(options, oflags, append) || base.cap == cap::null ||
        base.relpath == nullptr || *base.relpath == '\0')
    {
        return -1;
    }
    (void)append;

    CapIdx cap = sys_vfs_mkfile(base.cap, base.relpath, oflags);
    if (cap == cap::error || cap == cap::null) {
        return -1;
    }

    int fd = alloc_fd();
    if (fd < 0) {
        sys_cap_remove(cap);
        return -1;
    }

    g_files[fd].cap = cap;
    return fd;
}

void kmod_fclose(int fd) {
    auto *file = lookup_fd(fd);
    if (file == nullptr) {
        return;
    }
    if (file->cap != cap::null && file->cap != cap::error) {
        sys_cap_remove(file->cap);
    }
    *file = {};
}
}
