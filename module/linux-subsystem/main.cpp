/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief POSIX subsystem main file
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <elf.h>
#include <errno.h>
#include <prm.h>
#include <prog.h>
#include <std/stdio.h>
#include <sus/types.h>
#include <sustcore/bootstrap.h>
#include <sustcore/files.h>
#include <sustcore/syscall_str.h>
#include <syscall.h>

#include <cstddef>
#include <cstring>
#include <syscall.h.in>

#include "fdtable.h"

extern "C" bool g_linux_initialized = false;

extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7,
                                 addr_t dispatch_frame_sp);

#define INVALID_VALUE 0xFFFF'FFFF'FFFF'FFFF

namespace {
    constexpr size_t LINUX_MMAP_QUERY_BATCH = 64;
    constexpr size_t PROT_READ              = 0x1;
    constexpr size_t PROT_WRITE             = 0x2;
    constexpr size_t PROT_EXEC              = 0x4;
    constexpr size_t VMA_PROT_R             = 0x1;
    constexpr size_t VMA_PROT_W             = 0x2;
    constexpr size_t VMA_PROT_X             = 0x4;
    constexpr size_t MAP_PRIVATE            = 0x02;
    constexpr size_t MAP_FIXED              = 0x10;
    constexpr size_t MAP_ANONYMOUS          = 0x20;
    constexpr uint64_t MEMORY_GROWTH_FIXED  = 0;

    struct linux_iovec {
        const void *iov_base;
        size_t iov_len;
    };

    [[nodiscard]]
    size_t page_align_up_user(size_t value) noexcept {
        return (value + PAGESIZE - 1) & ~(PAGESIZE - 1);
    }

    [[nodiscard]]
    size_t prot_to_vma_prot(size_t prot) noexcept {
        size_t vma_prot = 0;
        if ((prot & PROT_READ) != 0) {
            vma_prot |= VMA_PROT_R;
        }
        if ((prot & PROT_WRITE) != 0) {
            vma_prot |= VMA_PROT_W;
        }
        if ((prot & PROT_EXEC) != 0) {
            vma_prot |= VMA_PROT_X;
        }
        return vma_prot;
    }

    [[nodiscard]]
    size_t choose_mmap_base(size_t length) {
        VMAInfo infos[LINUX_MMAP_QUERY_BATCH]{};
        size_t offset = 0;
        size_t cursor = page_align_up_user(__linuxss_ssheap_base + PAGESIZE);
        while (true) {
            size_t count = sys_pcb_query_vspace(__prog_pcb_cap, offset, infos,
                                                LINUX_MMAP_QUERY_BATCH);
            if (count == 0 || count == static_cast<size_t>(-1)) {
                return cursor;
            }
            for (size_t i = 0; i < count; ++i) {
                size_t start = reinterpret_cast<size_t>(infos[i].vma_start);
                size_t end   = start + infos[i].vma_size;
                if (cursor + length <= start) {
                    return cursor;
                }
                if (cursor < end) {
                    cursor = page_align_up_user(end);
                }
            }
            offset += count;
        }
    }

    constexpr int AT_FDCWD        = -100;
    constexpr int LINUX_O_RDONLY  = 0;
    constexpr int LINUX_O_WRONLY  = 1;
    constexpr int LINUX_O_RDWR    = 2;
    constexpr int LINUX_O_CREAT   = 0100;   // octal
    constexpr int LINUX_O_TRUNC   = 0200;   // octal
    constexpr int LINUX_O_APPEND  = 04000;  // octal

    [[nodiscard]]
    flags::oflg_t linux_oflags_to_sustcore(int linux_flags) noexcept {
        flags::oflg_t result = 0;
        int access_mode = linux_flags & 3;

        if (access_mode == LINUX_O_RDONLY) {
            result = flags::O_READ;
        } else if (access_mode == LINUX_O_WRONLY) {
            result = flags::O_WRITE;
        } else if (access_mode == LINUX_O_RDWR) {
            result = flags::O_READ | flags::O_WRITE;
        }

        if (linux_flags & LINUX_O_CREAT) {
            result |= flags::O_CREAT;
        }

        // O_TRUNC and O_APPEND intentionally not translated (out of scope)
        return result;
    }

}  // namespace

const char *at_to_string(int a_type) {
    switch (a_type) {
        case AT_NULL:              return "AT_NULL";
        case AT_IGNORE:            return "AT_IGNORE";
        case AT_EXECFD:            return "AT_EXECFD";
        case AT_PHDR:              return "AT_PHDR";
        case AT_PHENT:             return "AT_PHENT";
        case AT_PHNUM:             return "AT_PHNUM";
        case AT_PAGESZ:            return "AT_PAGESZ";
        case AT_BASE:              return "AT_BASE";
        case AT_FLAGS:             return "AT_FLAGS";
        case AT_ENTRY:             return "AT_ENTRY";
        case AT_NOTELF:            return "AT_NOTELF";
        case AT_UID:               return "AT_UID";
        case AT_EUID:              return "AT_EUID";
        case AT_GID:               return "AT_GID";
        case AT_EGID:              return "AT_EGID";
        case AT_PLATFORM:          return "AT_PLATFORM";
        case AT_HWCAP:             return "AT_HWCAP";
        case AT_CLKTCK:            return "AT_CLKTCK";
        case AT_FPUCW:             return "AT_FPUCW";
        case AT_DCACHEBSIZE:       return "AT_DCACHEBSIZE";
        case AT_ICACHEBSIZE:       return "AT_ICACHEBSIZE";
        case AT_UCACHEBSIZE:       return "AT_UCACHEBSIZE";
        case AT_IGNOREPPC:         return "AT_IGNOREPPC";
        case AT_SECURE:            return "AT_SECURE";
        case AT_BASE_PLATFORM:     return "AT_BASE_PLATFORM";
        case AT_RANDOM:            return "AT_RANDOM";
        case AT_HWCAP2:            return "AT_HWCAP2";
        case AT_RSEQ_FEATURE_SIZE: return "AT_RSEQ_FEATURE_SIZE";
        case AT_RSEQ_ALIGN:        return "AT_RSEQ_ALIGN";
        case AT_HWCAP3:            return "AT_HWCAP3";
        case AT_HWCAP4:            return "AT_HWCAP4";
        case AT_EXECFN:            return "AT_EXECFN";
        case AT_SYSINFO:           return "AT_SYSINFO";
        case AT_SYSINFO_EHDR:      return "AT_SYSINFO_EHDR";
        case AT_L1I_CACHESHAPE:    return "AT_L1I_CACHESHAPE";
        case AT_L1D_CACHESHAPE:    return "AT_L1D_CACHESHAPE";
        case AT_L2_CACHESHAPE:     return "AT_L2_CACHESHAPE";
        case AT_L3_CACHESHAPE:     return "AT_L3_CACHESHAPE";
        case AT_L1I_CACHESIZE:     return "AT_L1I_CACHESIZE";
        case AT_L1I_CACHEGEOMETRY: return "AT_L1I_CACHEGEOMETRY";
        case AT_L1D_CACHESIZE:     return "AT_L1D_CACHESIZE";
        case AT_L1D_CACHEGEOMETRY: return "AT_L1D_CACHEGEOMETRY";
        case AT_L2_CACHESIZE:      return "AT_L2_CACHESIZE";
        case AT_L2_CACHEGEOMETRY:  return "AT_L2_CACHEGEOMETRY";
        case AT_L3_CACHESIZE:      return "AT_L3_CACHESIZE";
        case AT_L3_CACHEGEOMETRY:  return "AT_L3_CACHEGEOMETRY";
        case AT_MINSIGSTKSZ:       return "AT_MINSIGSTKSZ";
        default:                   return "UNKNOWN";
    }
}

void dump_vector(const char *name, const char *const *items) {
    if (items == nullptr) {
        printf("%s: (null)\n", name);
        return;
    }
    for (size_t i = 0; items[i] != nullptr; ++i) {
        printf("%s[%u] = %s\n", name, static_cast<unsigned>(i), items[i]);
    }
}

void dump_auxv(const Elf64_auxv_t *auxv) {
    if (auxv == nullptr) {
        printf("auxv: (null)\n");
        return;
    }
    for (size_t i = 0; auxv[i].a_type != AT_NULL; ++i) {
        printf("auxv[%u] = { type=%s, val=%p }\n", static_cast<unsigned>(i),
               at_to_string(auxv[i].a_type),
               reinterpret_cast<void *>(auxv[i].a_un.a_val));
    }
    printf("auxv[end] = { type=%s, val=%p }\n", at_to_string(AT_NULL), nullptr);
}

void dump_bsargv(size_t bsargc, const bsheader *const *bsargv) {
    if (bsargv == nullptr) {
        printf("bsargv: (null)\n");
        return;
    }
    for (size_t i = 0; i < bsargc; ++i) {
        const bsheader *record = bsargv[i];
        if (record == nullptr) {
            printf("bsargv[%u] = nullptr\n", static_cast<unsigned>(i));
            continue;
        }
        BootstrapRecordView view{};
        if (!bootstrap_make_view(record, view)) {
            printf("bsargv[%u] = { type=%u, size=%u, invalid=true }\n",
                   static_cast<unsigned>(i), record->type, record->size);
            continue;
        }

        switch (record->type) {
            case boot::TYPE_CAPEXP: {
                BootstrapCapExplainView cap_view{};
                if (!bootstrap_parse_cap_explain(view, cap_view)) {
                    printf(
                        "bsargv[%u] = { type=TYPE_CAPEXP, size=%u, "
                        "parse_error=true }\n",
                        static_cast<unsigned>(i), record->size);
                    continue;
                }
                printf(
                    "bsargv[%u] = { type=TYPE_CAPEXP, size=%u, cap_idx=%lu, "
                    "cap_type=%s, cap_perm=0x%016lx, cap_desc=%s }\n",
                    static_cast<unsigned>(i), record->size, cap_view.cap_idx,
                    to_string(cap_view.cap_type), cap_view.cap_perm,
                    cap_view.cap_desc);
                continue;
            }
            case boot::TYPE_VADDREXP: {
                BootstrapVaddrExplainView vaddr_view{};
                if (!bootstrap_parse_vaddr_explain(view, vaddr_view)) {
                    printf(
                        "bsargv[%u] = { type=TYPE_VADDREXP, size=%u, "
                        "parse_error=true }\n",
                        static_cast<unsigned>(i), record->size);
                    continue;
                }
                printf(
                    "bsargv[%u] = { type=TYPE_VADDREXP, size=%u, vaddr=%p, "
                    "vaddr_desc=%s }\n",
                    static_cast<unsigned>(i), record->size,
                    vaddr_view.vaddr.addr(), vaddr_view.vaddr_desc);
                continue;
            }
            default:
                printf(
                    "bsargv[%u] = { type=%u, size=%u, raw_data=%p, "
                    "raw_size=%u }\n",
                    static_cast<unsigned>(i), record->type, record->size,
                    view.data, static_cast<unsigned>(view.data_size));
                continue;
        }
    }
}

void stack_dump(const void *stack_sp, const Elf64_auxv_t *auxv) {
    if (auxv == nullptr) {
        printf("stack_dump: auxv=null\n");
        return;
    }
    if (stack_sp == nullptr) {
        printf("stack_dump: stack_sp=null\n");
        return;
    }

    auto *begin = static_cast<const uint64_t *>(stack_sp);
    auto *end   = reinterpret_cast<const Elf64_auxv_t *>(auxv);
    while (end->a_type != AT_NULL) {
        ++end;
    }
    ++end;

    auto *word_end = reinterpret_cast<const uint64_t *>(end);
    for (auto *line = begin; line < word_end; line += 2) {
        printf("%p:", line);
        for (size_t i = 0; i < 2 && line + i < word_end; ++i) {
            printf(" 0x%016lx", line[i]);
        }
        printf("\n");
    }
}

extern "C" void linux_main(const void *stack_sp, size_t argc,
                           const char *argv[], const char *envp[],
                           const Elf64_auxv_t *auxv, size_t bsargc,
                           const bsheader *bsargv[]) {
    g_linux_initialized = true;
    init_prog_data(bsargc, bsargv);
    // puts("linux-subsystem: initialized");
    // printf("stack dump:\n");
    // stack_dump(stack_sp, auxv);
    // printf("argc & argv:\n");
    // printf("argc = %u\n", static_cast<unsigned>(argc));
    // dump_vector("argv", argv);
    // printf("\nenvp:\n");
    // dump_vector("envp", envp);
    // printf("\nauxv:\n");
    // dump_auxv(auxv);
    // printf("\nbsargc & bsargv:\n");
    // printf("bsargc = %u\n", static_cast<unsigned>(bsargc));
    // dump_bsargv(bsargc, bsargv);
}

size_t linux_sys_write(size_t fd, const void *buf, size_t len) {
    // Keep existing stdout/stderr serial behavior
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

    size_t offset    = fd_offset(static_cast<int>(fd));
    size_t written   = sys_vfs_write(file_cap, offset,
                                     reinterpret_cast<const void *>(buf), len);
    if (written == INVALID_VALUE) {
        return -EIO;
    }

    set_fd_offset(static_cast<int>(fd), offset + written);
    return written;
}

size_t linux_sys_read(int fd, void *buf, size_t count) {
    if (fd == 0) {
        // stdin not implemented
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

size_t linux_sys_writev(size_t fd, const linux_iovec *iov, size_t iovcnt) {
    if (fd != 1 && fd != 2) {
        printf("linux-subsystem: unsupported fd %d\n", fd);
        return INVALID_VALUE;
    }
    if (iov == nullptr && iovcnt != 0) {
        return INVALID_VALUE;
    }

    size_t total = 0;
    for (size_t i = 0; i < iovcnt; ++i) {
        if (iov[i].iov_base == nullptr && iov[i].iov_len != 0) {
            return INVALID_VALUE;
        }
        sys_write_serial(0, reinterpret_cast<const char *>(iov[i].iov_base),
                         iov[i].iov_len);
        total += iov[i].iov_len;
    }
    return total;
}

size_t linux_sys_mmap(void *addr, size_t length, size_t prot, size_t flags,
                      size_t fd, size_t offset) {
    if ((flags & MAP_ANONYMOUS) == 0 || (flags & MAP_PRIVATE) == 0) {
        return INVALID_VALUE;
    }
    if (fd != static_cast<size_t>(-1) || offset != 0 || length == 0) {
        return INVALID_VALUE;
    }

    size_t aligned_length = page_align_up_user(length);
    size_t target_addr    = (flags & MAP_FIXED) != 0
                                ? reinterpret_cast<size_t>(addr)
                                : choose_mmap_base(aligned_length);
    if ((target_addr % PAGESIZE) != 0) {
        return INVALID_VALUE;
    }

    CapIdx mem_cap =
        sys_mem_create(0, aligned_length, false, false, MEMORY_GROWTH_FIXED);
    if (mem_cap == cap::null || mem_cap == cap::error) {
        return INVALID_VALUE;
    }
    if (!sys_pcb_map(__prog_pcb_cap, mem_cap, 0,
                     reinterpret_cast<void *>(target_addr), aligned_length,
                     prot_to_vma_prot(prot)))
    {
        return INVALID_VALUE;
    }
    return target_addr;
}

size_t linux_sys_munmap(void *addr, size_t length) {
    if (addr == nullptr || length == 0) {
        return INVALID_VALUE;
    }
    size_t aligned_length = page_align_up_user(length);
    size_t target_addr    = reinterpret_cast<size_t>(addr);
    if ((target_addr % PAGESIZE) != 0) {
        return INVALID_VALUE;
    }
    return sys_pcb_unmap(__prog_pcb_cap, addr, aligned_length) ? 0
                                                               : INVALID_VALUE;
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
    CapIdx parent_cap;
    if (dirfd == AT_FDCWD) {
        parent_cap = __prog_root_dir_cap;
    } else {
        return -ENOENT;
    }

    if (parent_cap == cap::null) {
        return -ENOENT;
    }

    if (pathname == nullptr) {
        return -ENOENT;
    }

    flags::oflg_t sustcore_flags = linux_oflags_to_sustcore(flags);

    CapIdx file_cap = sys_vfs_open(parent_cap, pathname, sustcore_flags);

    if (file_cap == cap::null || file_cap == cap::error) {
        return -ENOENT;
    }

    int fd = alloc_fd(file_cap);
    if (fd < 0) {
        sys_cap_remove(file_cap);
        return -EMFILE;
    }

    return fd;
}

size_t linux_sys_lseek(int fd, size_t offset, int whence) {
    CapIdx file_cap = fd_to_cap(fd);
    if (file_cap == cap::error) {
        return -EBADF;
    }

    size_t new_offset;

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

extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7,
                                 addr_t dispatch_frame_sp) {
    switch (a7) {
        case __NR_write:
            return linux_sys_write(a0, reinterpret_cast<const void *>(a1), a2);
        case __NR_read:
            return linux_sys_read(static_cast<int>(a0),
                                  reinterpret_cast<void *>(a1), a2);
        case __NR_writev:
            return linux_sys_writev(
                a0, reinterpret_cast<const linux_iovec *>(a1), a2);
        case __NR_mmap:
            return linux_sys_mmap(reinterpret_cast<void *>(a0), a1, a2, a3, a4,
                                  a5);
        case __NR_munmap:
            return linux_sys_munmap(reinterpret_cast<void *>(a0), a1);
        case __NR_clone:
            return linux_sys_clone(a0, a1, reinterpret_cast<int *>(a2),
                                   reinterpret_cast<int *>(a3), a4,
                                   dispatch_frame_sp);
        case __NR_brk: {
            return linux_sys_brk(a0);
        }
        case __NR_uname: return linux_sys_uname(reinterpret_cast<void *>(a0));
        case __NR_faccessat:
            // TODO: 实现 __NR_faccessat 系统调用，目前先返回 -ENOENT;
            return -ENOENT;
        case __NR_set_tid_address:
            // 未实现
            return -ENOSYS;
        case __NR_set_robust_list:
            // 未实现
            return -ENOSYS;
        case __NR_wait4:
            return linux_sys_wait4(static_cast<int>(a0),
                                   reinterpret_cast<int *>(a1),
                                   static_cast<int>(a2),
                                   reinterpret_cast<void *>(a3));
        case __NR_exit: linux_sys_exit(a0); return 0;
        case __NR_openat:
            return linux_sys_openat(static_cast<int>(a0),
                                    reinterpret_cast<const char *>(a1),
                                    static_cast<int>(a2), static_cast<int>(a3));
        case __NR_close:
            return linux_sys_close(static_cast<int>(a0));
        case __NR_lseek:
            return linux_sys_lseek(static_cast<int>(a0), a1,
                                   static_cast<int>(a2));
        default:
            printf("linux-subsystem: unsupported syscall %s (%d)\n",
                   syscall_to_string(a7), a7);
            printf(
                "linux-subsystem: syscall arguments: a0=%p, a1=%p, a2=%p, "
                "a3=%p, "
                "a4=%p, a5=%p, a6=%p\n",
                reinterpret_cast<void *>(a0),
                reinterpret_cast<const char *>(a1),
                reinterpret_cast<void *>(a2), reinterpret_cast<void *>(a3),
                reinterpret_cast<void *>(a4), reinterpret_cast<void *>(a5),
                reinterpret_cast<void *>(a6));
            // 直接退出
            return -ENOSYS;
    }
}
