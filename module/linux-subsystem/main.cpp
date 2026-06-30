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
#include <logger.h>
#include <prm.h>
#include <prog.h>
#include <std/stdio.h>
#include <sus/types.h>
#include <sustcore/bootstrap.h>
#include <sustcore/syscall_str.h>
#include <syscall.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <syscall.h.in>

#include "fdtable.h"
#include "file.h"
#include "pipe.h"

extern "C" bool g_linux_initialized = false;

extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7,
                                 addr_t dispatch_frame_sp);

#define INVALID_VALUE 0xFFFF'FFFF'FFFF'FFFF

namespace {
    constexpr size_t LINUX_MMAP_QUERY_BATCH = 64;
    constexpr size_t LINUX_MMAP_GAP         = 0x40000000ULL;
    constexpr size_t LINUX_MMAP_SCAN_LIMIT  = 0x1000000000ULL;
    constexpr size_t PROT_READ              = 0x1;
    constexpr size_t PROT_WRITE             = 0x2;
    constexpr size_t PROT_EXEC              = 0x4;
    constexpr size_t VMA_PROT_R             = 0x1;
    constexpr size_t VMA_PROT_W             = 0x2;
    constexpr size_t VMA_PROT_X             = 0x4;
    constexpr size_t VMA_PROT_SHARE         = 0x8;

    // 共享映射, 修改同步到文件并对其他进程可见
    constexpr size_t MAP_SHARED          = 0x01;
    // 私有映射 (写时复制), 修改不写回文件
    constexpr size_t MAP_PRIVATE         = 0x02;
    // 验证额外标志是否被内核支持 (需与 MAP_SHARED 联用)
    constexpr size_t MAP_SHARED_VALIDATE = 0x03;
    // 固定地址映射, 若指定地址不可用则失败
    constexpr size_t MAP_FIXED           = 0x10;
    // 匿名映射, 不与文件关联, 内容初始化为 0
    constexpr size_t MAP_ANONYMOUS       = 0x20;
    // 将映射放在地址空间的低 2GB (仅 x86-64)
    constexpr size_t MAP_32BIT           = 0x40;
    // 映射可向下增长 (通常用于栈)
    constexpr size_t MAP_GROWSDOWN       = 0x00100;
    // 拒绝写入该文件 (忽略)
    constexpr size_t MAP_DENYWRITE       = 0x00800;
    // 标记为可执行 (忽略)
    constexpr size_t MAP_EXECUTABLE      = 0x01000;
    // 锁定内存页, 防止被交换到 swap
    constexpr size_t MAP_LOCKED          = 0x02000;
    // 不预留交换空间 (可能因内存不足导致 SIGSEGV)
    constexpr size_t MAP_NORESERVE       = 0x04000;
    // 预填充页表 (预读文件), 减少后续缺页中断
    constexpr size_t MAP_POPULATE        = 0x08000;
    // 在 I/O 操作时不要阻塞 (与 MAP_POPULATE 联用)
    constexpr size_t MAP_NONBLOCK        = 0x10000;
    // 用于线程栈的映射
    constexpr size_t MAP_STACK           = 0x20000;
    // 使用大页 (Huge Page)
    constexpr size_t MAP_HUGETLB         = 0x40000;
    // DAX 持久性保证 (需与 MAP_SHARED_VALIDATE 联用)
    constexpr size_t MAP_SYNC            = 0x80000;
    // 固定地址映射, 但绝不替换已有映射 (若冲突则失败)
    constexpr size_t MAP_FIXED_NOREPLACE = 0x100000;
    constexpr int MS_ASYNC               = 1;
    constexpr int MS_INVALIDATE          = 2;
    constexpr int MS_SYNC                = 4;

    constexpr uint64_t MEMORY_GROWTH_FIXED = 0;
    constexpr uint64_t LINUX_SA_SIGINFO    = 0x00000004UL;
    constexpr uint64_t LINUX_SA_RESTORER   = 0x04000000UL;

    struct linux_sigset_t {
        uint64_t sig[1];
    };

    struct linux_sigaction {
        size_t handler;
        uint64_t flags;
        size_t restorer;
        linux_sigset_t mask;
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

    void append_flag_name(std::string &out, bool &first, const char *name) {
        if (!first) {
            out += " | ";
        }
        out   += name;
        first  = false;
    }

    [[nodiscard]]
    std::string mmap_prot_to_string(size_t prot) {
        std::string out{};
        bool first = true;

        if ((prot & PROT_READ) != 0) {
            append_flag_name(out, first, "PROT_READ");
        }
        if ((prot & PROT_WRITE) != 0) {
            append_flag_name(out, first, "PROT_WRITE");
        }
        if ((prot & PROT_EXEC) != 0) {
            append_flag_name(out, first, "PROT_EXEC");
        }

        size_t unknown = prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC);
        if (unknown != 0) {
            char buffer[32]{};
            snprintf(buffer, sizeof(buffer), "0x%lx",
                     static_cast<unsigned long>(unknown));
            append_flag_name(out, first, buffer);
        }

        if (out.empty()) {
            out = "0";
        }
        return out;
    }

    [[nodiscard]]
    std::string mmap_flags_to_string(size_t flags) {
        std::string out{};
        bool first       = true;
        size_t rem_flags = flags;

        struct FlagMap {
            size_t mask;
            const char *name;
        };

        if ((rem_flags & MAP_SHARED_VALIDATE) == MAP_SHARED_VALIDATE) {
            append_flag_name(out, first, "MAP_SHARED_VALIDATE");
            rem_flags &= ~MAP_SHARED_VALIDATE;
        } else if ((rem_flags & MAP_SHARED) != 0) {
            append_flag_name(out, first, "MAP_SHARED");
            rem_flags &= ~MAP_SHARED;
        }

        const FlagMap flag_map[] = {
            {.mask = MAP_PRIVATE, .name = "MAP_PRIVATE"},
            {.mask = MAP_FIXED, .name = "MAP_FIXED"},
            {.mask = MAP_ANONYMOUS, .name = "MAP_ANONYMOUS"},
            {.mask = MAP_32BIT, .name = "MAP_32BIT"},
            {.mask = MAP_GROWSDOWN, .name = "MAP_GROWSDOWN"},
            {.mask = MAP_DENYWRITE, .name = "MAP_DENYWRITE"},
            {.mask = MAP_EXECUTABLE, .name = "MAP_EXECUTABLE"},
            {.mask = MAP_LOCKED, .name = "MAP_LOCKED"},
            {.mask = MAP_NORESERVE, .name = "MAP_NORESERVE"},
            {.mask = MAP_POPULATE, .name = "MAP_POPULATE"},
            {.mask = MAP_NONBLOCK, .name = "MAP_NONBLOCK"},
            {.mask = MAP_STACK, .name = "MAP_STACK"},
            {.mask = MAP_HUGETLB, .name = "MAP_HUGETLB"},
            {.mask = MAP_SYNC, .name = "MAP_SYNC"},
            {.mask = MAP_FIXED_NOREPLACE, .name = "MAP_FIXED_NOREPLACE"},
        };

        for (const auto &[mask, name] : flag_map) {
            if ((rem_flags & mask) == 0) {
                continue;
            }
            append_flag_name(out, first, name);
            rem_flags &= ~mask;
        }

        if (rem_flags != 0) {
            char buffer[32]{};
            snprintf(buffer, sizeof(buffer), "0x%lx",
                     static_cast<unsigned long>(rem_flags));
            append_flag_name(out, first, buffer);
        }

        if (out.empty()) {
            out = "0";
        }
        return out;
    }

    [[nodiscard]]
    size_t choose_mmap_base(size_t length) {
        VMAInfo infos[LINUX_MMAP_QUERY_BATCH]{};
        size_t offset  = 0;
        size_t max_end = 0;
        while (true) {
            auto count_res = sys_pcb_query_vspace(__prog_pcb_cap, offset, infos,
                                                  LINUX_MMAP_QUERY_BATCH)
                                 .to_result();
            if (!count_res.has_value()) {
                break;
            }
            size_t count = count_res.value();
            if (count == 0) {
                break;
            }
            for (size_t i = 0; i < count; ++i) {
                size_t start = reinterpret_cast<size_t>(infos[i].vma_start);
                if (start > LINUX_MMAP_SCAN_LIMIT) {
                    continue;
                }
                size_t end = start + infos[i].vma_size;
                if (end > max_end) {
                    max_end = end;
                }
            }
            offset += count;
        }

        size_t cursor = page_align_up_user(max_end + LINUX_MMAP_GAP);
        offset        = 0;
        while (true) {
            auto count_res = sys_pcb_query_vspace(__prog_pcb_cap, offset, infos,
                                                  LINUX_MMAP_QUERY_BATCH)
                                 .to_result();
            if (!count_res.has_value()) {
                return cursor;
            }
            size_t count = count_res.value();
            if (count == 0) {
                return cursor;
            }
            for (size_t i = 0; i < count; ++i) {
                size_t start = reinterpret_cast<size_t>(infos[i].vma_start);
                if (start > LINUX_MMAP_SCAN_LIMIT) {
                    continue;
                }
                size_t end = start + infos[i].vma_size;
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

    [[nodiscard]]
    bool mmap_range_conflicts(size_t begin, size_t length) {
        if (length == 0) {
            return false;
        }

        VMAInfo infos[LINUX_MMAP_QUERY_BATCH]{};
        size_t offset = 0;
        size_t end    = begin + length;
        while (true) {
            auto count_res = sys_pcb_query_vspace(__prog_pcb_cap, offset, infos,
                                                  LINUX_MMAP_QUERY_BATCH)
                                 .to_result();
            if (!count_res.has_value()) {
                return false;
            }
            size_t count = count_res.value();
            if (count == 0) {
                return false;
            }
            for (size_t i = 0; i < count; ++i) {
                size_t vma_begin = reinterpret_cast<size_t>(infos[i].vma_start);
                size_t vma_end   = vma_begin + infos[i].vma_size;
                if (begin < vma_end && vma_begin < end) {
                    return true;
                }
            }
            offset += count;
        }
    }

    [[nodiscard]]
    size_t linux_rt_sigaction(int signo, const linux_sigaction *act,
                              linux_sigaction *oldact,
                              size_t sigsetsize) {
        (void)LINUX_SA_SIGINFO;
        if (sigsetsize < sizeof(linux_sigset_t)) {
            return static_cast<size_t>(-EINVAL);
        }
        if (signo <= 0 || static_cast<size_t>(signo) >= 64) {
            return static_cast<size_t>(-EINVAL);
        }

        KmodSigAction action{};
        KmodSigAction old_action{};
        KmodSigAction *action_ptr = nullptr;
        KmodSigAction *old_ptr    = oldact != nullptr ? &old_action : nullptr;

        if (act != nullptr) {
            action.handler  = act->handler;
            action.flags    = act->flags;
            action.restorer = act->restorer;
            action.mask     = act->mask.sig[0] << 1U;
            const bool is_user_handler =
                action.handler != 0 && action.handler != 1;
            if (is_user_handler) {
                if ((action.flags & LINUX_SA_RESTORER) != 0) {
                    if (action.restorer == 0) {
                        return static_cast<size_t>(-EINVAL);
                    }
                } else {
                    action.flags |= LINUX_SA_RESTORER;
                    action.restorer = reinterpret_cast<size_t>(
                        &linux_signal_rt_sigreturn_trampoline);
                }
            }
            action_ptr      = &action;
        }

        auto sigaction_res =
            sys_pcb_sigaction(__prog_pcb_cap, static_cast<size_t>(signo),
                              action_ptr, old_ptr)
                .to_result();
        if (!sigaction_res.has_value()) {
            loggers::LXSC::ERROR("rt_sigaction failed signo=%d err=%s", signo,
                                 to_cstring(sigaction_res.error()));
            return static_cast<size_t>(-EINVAL);
        }

        if (oldact != nullptr) {
            memset(oldact, 0, sizeof(*oldact));
            oldact->handler     = old_action.handler;
            oldact->flags       = old_action.flags;
            oldact->restorer    = old_action.restorer;
            oldact->mask.sig[0] = old_action.mask >> 1U;
        }
        return 0;
    }

    [[nodiscard]]
    size_t linux_sys_msync(void *addr, size_t length, int flags) {
        if (addr == nullptr || length == 0) {
            return static_cast<size_t>(-EINVAL);
        }
        size_t target_addr = reinterpret_cast<size_t>(addr);
        if ((target_addr % PAGESIZE) != 0) {
            return static_cast<size_t>(-EINVAL);
        }
        if ((flags & ~(MS_ASYNC | MS_INVALIDATE | MS_SYNC)) != 0) {
            return static_cast<size_t>(-EINVAL);
        }

        VMAInfo infos[LINUX_MMAP_QUERY_BATCH]{};
        size_t offset        = 0;
        size_t sync_begin    = target_addr;
        size_t sync_end      = page_align_up_user(target_addr + length);
        bool touched_mapping = false;

        while (true) {
            auto count_res = sys_pcb_query_vspace(__prog_pcb_cap, offset, infos,
                                                  LINUX_MMAP_QUERY_BATCH)
                                 .to_result();
            if (!count_res.has_value()) {
                return touched_mapping ? 0 : static_cast<size_t>(-EINVAL);
            }
            size_t count = count_res.value();
            if (count == 0) {
                return touched_mapping ? 0 : static_cast<size_t>(-EINVAL);
            }

            for (size_t i = 0; i < count; ++i) {
                size_t vma_begin = reinterpret_cast<size_t>(infos[i].vma_start);
                size_t vma_end   = vma_begin + infos[i].vma_size;
                if (sync_end <= vma_begin || vma_end <= sync_begin) {
                    continue;
                }
                touched_mapping = true;
                if ((infos[i].vma_prot & VMA_PROT_SHARE) == 0 ||
                    infos[i].mem_cap == cap::null || infos[i].mem_cap == cap::error)
                {
                    continue;
                }
                size_t part_begin = sync_begin > vma_begin ? sync_begin : vma_begin;
                size_t part_end   = sync_end < vma_end ? sync_end : vma_end;
                auto sync_res =
                    sys_mem_sync(infos[i].mem_cap, part_begin - vma_begin,
                                 part_end - part_begin)
                        .to_result();
                if (!sync_res.has_value()) {
                    return static_cast<size_t>(-EIO);
                }
            }
            offset += count;
        }
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
            case boot::TYPE_PATHEXP: {
                BootstrapPathExplainView path_view{};
                if (!bootstrap_parse_path_explain(view, path_view)) {
                    printf(
                        "bsargv[%u] = { type=TYPE_PATHEXP, size=%u, "
                        "parse_error=true }\n",
                        static_cast<unsigned>(i), record->size);
                    continue;
                }
                printf(
                    "bsargv[%u] = { type=TYPE_PATHEXP, size=%u, path_desc=%s "
                    "}\n",
                    static_cast<unsigned>(i), record->size,
                    path_view.path_desc);
                continue;
            }
            case boot::TYPE_SHELLIO: {
                BootstrapShellIoView shellio_view{};
                if (!bootstrap_parse_shell_io(view, shellio_view)) {
                    printf(
                        "bsargv[%u] = { type=TYPE_SHELLIO, size=%u, "
                        "parse_error=true }\n",
                        static_cast<unsigned>(i), record->size);
                    continue;
                }
                printf(
                    "bsargv[%u] = { type=TYPE_SHELLIO, size=%u, "
                    "shellio_target=%d, shellio_flags=%d}\n",
                    static_cast<unsigned>(i), record->size, shellio_view.target,
                    shellio_view.flags);
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
    init_prog_data(argc, argv, bsargc, bsargv);
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

size_t linux_sys_mmap(void *addr, size_t length, size_t prot, size_t flags,
                      size_t fd, size_t offset) {
    auto prot_str  = mmap_prot_to_string(prot);
    auto flags_str = mmap_flags_to_string(flags);
    loggers::LXSC::INFO(
        "mmap addr=%p length=%lu prot=0x%lx (%s) flags=0x%lx (%s) fd=%ld "
        "offset=%lu",
        addr, static_cast<unsigned long>(length),
        static_cast<unsigned long>(prot), prot_str.c_str(),
        static_cast<unsigned long>(flags), flags_str.c_str(),
        static_cast<long>(fd), static_cast<unsigned long>(offset));

    bool is_anonymous = (flags & MAP_ANONYMOUS) != 0;
    bool is_fixed     = (flags & MAP_FIXED) != 0;
    bool no_replace   = (flags & MAP_FIXED_NOREPLACE) != 0;
    bool is_private   = (flags & MAP_PRIVATE) != 0;
    bool is_shared    = (flags & MAP_SHARED) != 0;

    if (!is_anonymous && fd == static_cast<size_t>(-1)) {
        loggers::LXSC::ERROR(
            "mmap requires either MAP_ANONYMOUS or a valid fd, flags=0x%lx, "
            "fd=%ld",
            static_cast<unsigned long>(flags), static_cast<long>(fd));
        return INVALID_VALUE;
    }

    if (!(is_private ^ is_shared)) {
        loggers::LXSC::ERROR(
            "mmap requires either MAP_PRIVATE or MAP_SHARED, flags=0x%lx",
            static_cast<unsigned long>(flags));
        return INVALID_VALUE;
    }

    if ((offset % PAGESIZE) != 0) {
        loggers::LXSC::ERROR("mmap requires page-aligned offset, offset=%lu",
                             static_cast<unsigned long>(offset));
        return INVALID_VALUE;
    }

    if (length == 0) {
        loggers::LXSC::ERROR("mmap requires length > 0");
        return INVALID_VALUE;
    }

    size_t aligned_length = page_align_up_user(length);
    size_t target_addr    = is_fixed ? reinterpret_cast<size_t>(addr)
                                     : choose_mmap_base(aligned_length);

    if (!is_fixed) {
        loggers::LXSC::INFO(
            "mmap chose target address=[%p, %p) for length=%lu",
            reinterpret_cast<void *>(target_addr),
            reinterpret_cast<void *>(target_addr + aligned_length),
            static_cast<unsigned long>(aligned_length));
    }

    if ((target_addr % PAGESIZE) != 0) {
        loggers::LXSC::ERROR("mmap target address is not page-aligned, addr=%p",
                             reinterpret_cast<void *>(target_addr));
        return INVALID_VALUE;
    }

    if (is_fixed) {
        bool conflict = mmap_range_conflicts(target_addr, aligned_length);
        if (conflict && no_replace) {
            loggers::LXSC::ERROR(
                "mmap MAP_FIXED_NOREPLACE conflicts with existing VMA, "
                "range=[%p, %p)",
                reinterpret_cast<void *>(target_addr),
                reinterpret_cast<void *>(target_addr + aligned_length));
            return INVALID_VALUE;
        }
        if (conflict) {
            loggers::LXSC::INFO(
                "mmap MAP_FIXED removes conflicting range=[%p, %p)",
                reinterpret_cast<void *>(target_addr),
                reinterpret_cast<void *>(target_addr + aligned_length));
            if (!sys_pcb_unmap(__prog_pcb_cap,
                               reinterpret_cast<void *>(target_addr),
                               aligned_length))
            {
                loggers::LXSC::ERROR(
                    "mmap MAP_FIXED failed to unmap conflicting range");
                return INVALID_VALUE;
            }
        }
    }

    CapIdx backing_file_cap = cap::null;
    size_t file_offset      = 0;
    if (is_anonymous) {
        if (fd != static_cast<size_t>(-1)) {
            loggers::LXSC::WARN(
                "anonymous mmap requires fd=-1, got fd=%ld, ignored",
                static_cast<long>(fd));
            fd = -1;
        }
    } else {
        backing_file_cap = fd_to_cap(static_cast<int>(fd));
        if (backing_file_cap == cap::null || backing_file_cap == cap::error) {
            loggers::LXSC::ERROR("file-backed mmap invalid fd=%ld",
                                 static_cast<long>(fd));
            return INVALID_VALUE;
        }
        file_offset = offset;
    }

    auto mem_cap_res = sys_mem_create(backing_file_cap, aligned_length, is_shared,
                                      false, MEMORY_GROWTH_FIXED, file_offset)
                           .to_result();
    if (!mem_cap_res.has_value()) {
        loggers::LXSC::ERROR("mmap failed to create memory capability");
        return INVALID_VALUE;
    }
    CapIdx mem_cap = mem_cap_res.value();
    size_t vma_prot = prot_to_vma_prot(prot);
    if (is_shared) {
        vma_prot |= VMA_PROT_SHARE;
    }
    if (!sys_pcb_map(__prog_pcb_cap, mem_cap, 0,
                     reinterpret_cast<void *>(target_addr), aligned_length,
                     vma_prot))
    {
        loggers::LXSC::ERROR("mmap failed to map memory capability to vspace");
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
    loggers::LXSC::INFO("munmap addr=%p length=%lu", addr,
                        static_cast<unsigned long>(length));
    return sys_pcb_unmap(__prog_pcb_cap, addr, aligned_length) ? 0
                                                               : INVALID_VALUE;
}

extern "C" size_t linux_dispatch(size_t a0, size_t a1, size_t a2, size_t a3,
                                 size_t a4, size_t a5, size_t a6, size_t a7,
                                 addr_t dispatch_frame_sp) {
    // printf("linux syscall %s (%lu)\n", syscall_to_string(a7), a7);
    switch (a7) {
        case __NR_write:
            return linux_sys_write(a0, reinterpret_cast<const void *>(a1), a2);
        case __NR_read:
            return linux_sys_read(static_cast<int>(a0),
                                  reinterpret_cast<void *>(a1), a2);
        case __NR_pipe2:
            return linux_sys_pipe2(reinterpret_cast<int *>(a0),
                                   static_cast<int>(a1));
        case __NR_writev:
            return linux_sys_writev(static_cast<int>(a0),
                                    reinterpret_cast<const void *>(a1),
                                    static_cast<int>(a2));
        case __NR_readv:
            return linux_sys_readv(static_cast<int>(a0),
                                   reinterpret_cast<const void *>(a1),
                                   static_cast<int>(a2));
        case __NR_mmap:
            return linux_sys_mmap(reinterpret_cast<void *>(a0), a1, a2, a3, a4,
                                  a5);
        case __NR_munmap:
            return linux_sys_munmap(reinterpret_cast<void *>(a0), a1);
        case __NR_msync:
            return linux_sys_msync(reinterpret_cast<void *>(a0), a1,
                                   static_cast<int>(a2));
        case __NR_clone:
            return linux_sys_clone(a0, a1, reinterpret_cast<int *>(a2),
                                   reinterpret_cast<int *>(a3), a4,
                                   dispatch_frame_sp);
        case __NR_execve:
            return linux_sys_execve(reinterpret_cast<const char *>(a0),
                                    reinterpret_cast<const char *const *>(a1),
                                    reinterpret_cast<const char *const *>(a2));
        case __NR_brk:   return linux_sys_brk(a0);
        case __NR_uname: return linux_sys_uname(reinterpret_cast<void *>(a0));
        case __NR_faccessat:
            // TODO: 实现 __NR_faccessat 系统调用, 目前先返回 -ENOENT;
            return -ENOENT;
        case __NR_wait4:
            return linux_sys_wait4(
                static_cast<int>(a0), reinterpret_cast<int *>(a1),
                static_cast<int>(a2), reinterpret_cast<void *>(a3));
        case __NR_rt_sigtimedwait:
            return linux_sys_rt_sigtimedwait(reinterpret_cast<const void *>(a0),
                                             reinterpret_cast<void *>(a1),
                                             reinterpret_cast<const void *>(a2),
                                             a3);
        case __NR_gettimeofday:
            return linux_sys_gettimeofday(reinterpret_cast<void *>(a0),
                                          reinterpret_cast<void *>(a1));
        case __NR_nanosleep:
            // 容易导致系统不稳定
            return 0;
            return linux_sys_nanosleep(reinterpret_cast<const void *>(a0),
                                       reinterpret_cast<void *>(a1));
        case __NR_getpid:  return linux_sys_getpid();
        case __NR_getppid: return linux_sys_getppid();
        case __NR_gettid:  return linux_sys_gettid();
        case __NR_kill:
            return linux_sys_kill(static_cast<int>(a0), static_cast<int>(a1));
        case __NR_tgkill:
            return linux_sys_tgkill(static_cast<int>(a0), static_cast<int>(a1),
                                    static_cast<int>(a2));
        case __NR_sched_yield: return linux_sys_sched_yield();
        case __NR_times:       return linux_sys_times(reinterpret_cast<void *>(a0));
        case __NR_chdir:
            return linux_sys_chdir(reinterpret_cast<const char *>(a0));
        case __NR_getdents64:
            return linux_sys_getdents64(static_cast<int>(a0),
                                        reinterpret_cast<void *>(a1), a2);
        case __NR_readlinkat:
            return linux_sys_readlinkat(static_cast<int>(a0),
                                        reinterpret_cast<const char *>(a1),
                                        reinterpret_cast<char *>(a2), a3);
        case __NR_symlinkat:
            return linux_sys_symlinkat(reinterpret_cast<const char *>(a0),
                                       static_cast<int>(a1),
                                       reinterpret_cast<const char *>(a2));
        case __NR_getcwd:
            return linux_sys_getcwd(reinterpret_cast<char *>(a0), a1);
        case __NR_getrandom:
            return linux_sys_getrandom(reinterpret_cast<void *>(a0), a1,
                                       static_cast<unsigned>(a2));
        case __NR_personality:
            return linux_sys_personality(static_cast<unsigned long>(a0));
        case __NR_fstat:
            return linux_sys_fstat(static_cast<int>(a0),
                                   reinterpret_cast<void *>(a1));
        case __NR_statfs:
            return linux_sys_statfs(reinterpret_cast<const char *>(a0),
                                    reinterpret_cast<void *>(a1));
        case __NR_fstatfs:
            return linux_sys_fstatfs(static_cast<int>(a0),
                                     reinterpret_cast<void *>(a1));
        case __NR_newfstatat:
            return linux_sys_newfstatat(
                static_cast<int>(a0), reinterpret_cast<const char *>(a1),
                reinterpret_cast<void *>(a2), static_cast<int>(a3));
        case __NR_statx:
            return linux_sys_statx(
                static_cast<int>(a0), reinterpret_cast<const char *>(a1),
                static_cast<int>(a2), static_cast<unsigned>(a3),
                reinterpret_cast<void *>(a4));
        case __NR_exit:
            // 先注释, 默认均为 exit_group
            // linux_sys_exit(a0);
            // return 0;
        case __NR_exit_group:
            linux_sys_exit_group(a0);
            return 0;
        case __NR_close: return linux_sys_close(static_cast<int>(a0));
        case __NR_dup:   return linux_sys_dup(static_cast<int>(a0));
        case __NR_dup3:
            return linux_sys_dup3(static_cast<int>(a0), static_cast<int>(a1),
                                  static_cast<int>(a2));
        case __NR_fcntl:
            return linux_sys_fcntl(static_cast<int>(a0), static_cast<int>(a1),
                                   a2);
        case __NR_ioctl: return linux_sys_ioctl(static_cast<int>(a0), a1, a2);
        case __NR_lseek:
            return linux_sys_lseek(static_cast<int>(a0), a1,
                                   static_cast<int>(a2));
        case __NR_ftruncate:
            return linux_sys_ftruncate(static_cast<int>(a0), a1);
        case __NR_fallocate:
            return linux_sys_fallocate(static_cast<int>(a0),
                                       static_cast<int>(a1), a2, a3);
        case __NR_openat:
            return linux_sys_openat(static_cast<int>(a0),
                                    reinterpret_cast<const char *>(a1),
                                    static_cast<int>(a2), static_cast<int>(a3));
        case __NR_fchmodat:
            return linux_sys_fchmodat(static_cast<int>(a0),
                                      reinterpret_cast<const char *>(a1),
                                      static_cast<uint32_t>(a2));
        case __NR_fchownat:
            return linux_sys_fchownat(
                static_cast<int>(a0), reinterpret_cast<const char *>(a1),
                static_cast<uint32_t>(a2), static_cast<uint32_t>(a3),
                static_cast<int>(a4));
        case __NR_unlinkat:
            return linux_sys_unlinkat(static_cast<int>(a0),
                                      reinterpret_cast<const char *>(a1),
                                      static_cast<int>(a2));
        case __NR_mkdirat:
            return linux_sys_mkdirat(static_cast<int>(a0),
                                     reinterpret_cast<const char *>(a1),
                                     static_cast<int>(a2));
        case __NR_renameat2:
            return linux_sys_renameat2(
                static_cast<int>(a0), reinterpret_cast<const char *>(a1),
                static_cast<int>(a2), reinterpret_cast<const char *>(a3),
                static_cast<unsigned int>(a4));
        case __NR_mount:
        case __NR_umount2:
            // 占位符
            // 等后面支持分区 + vfat 了再写入实际的实现
        case __NR_mprotect:
            // 占位符
            // 我假设其总是把内存页的权限设置为可读可写可执行之后再缩减权限
            // 那么我先不缩减权限应该也不会影响程序的运行
        case __NR_getuid:
        case __NR_setuid:
        case __NR_getgid:
        case __NR_setgid:
        case __NR_geteuid:
        case __NR_getegid:
        case __NR_getresuid:
        case __NR_setresuid:
        case __NR_getresgid:
        case __NR_setresgid:
        case __NR_setpgid:
        case __NR_getpgid:
        case __NR_setfsgid:
        case __NR_setfsuid:
        case __NR_setreuid:
            // 占位符
            // 先假设所有的 uid/gid 都是 0
        case __NR_prlimit64:
            // 占位符
            // 先假设所有的资源限制都是无限制的
        case __NR_set_robust_list:
            // 占位符
        case __NR_utimensat:
            // 占位符
            loggers::LXSC::WARN(
                "unsupported syscall %s (%lu), but returning 0 for "
                "compatibility",
                syscall_to_string(a7), a7);
            return 0;
        case __NR_syslog:
            return linux_sys_syslog(static_cast<int>(a0),
                                    reinterpret_cast<void *>(a1),
                                    static_cast<int>(a2));
        case __NR_set_tid_address: {
            loggers::LXSC::WARN(
                "unsupported syscall %s (%lu), ignoring ptr=%p and forwarding "
                "to main tcb tid",
                syscall_to_string(a7), a7, reinterpret_cast<void *>(a0));
            auto tid_res = sys_tcb_get_tid(__prog_main_tcb_cap).to_result();
            if (!tid_res.has_value()) {
                loggers::LXSC::ERROR("sys_tcb_get_tid(main_tcb) failed: err=%s",
                                     to_cstring(tid_res.error()));
                return -EINVAL;
            }
            return tid_res.value();
        }
        case __NR_sched_getaffinity:
            loggers::LXSC::WARN(
                "unsupported syscall %s (%lu), ignoring pid=%d and returning 1 "
                "for compatibility",
                syscall_to_string(a7), a7, static_cast<int>(a0));
            return 1;
        case __NR_rt_sigaction:
            return linux_rt_sigaction(static_cast<int>(a0),
                                      reinterpret_cast<const linux_sigaction *>(a1),
                                      reinterpret_cast<linux_sigaction *>(a2),
                                      a3);
        case __NR_setitimer:
            loggers::LXSC::WARN(
                "unsupported syscall %s (%lu) with which=%d, ignoring and returning 0 for "
                "compatibility",
                syscall_to_string(a7), a7, static_cast<int>(a0));
            return 0;
        case __NR_clock_gettime:
            return linux_sys_clock_gettime(static_cast<int>(a0),
                                           reinterpret_cast<void *>(a1));
        default:
            loggers::LXSC::ERROR("unsupported syscall %s (%lu)",
                                 syscall_to_string(a7), a7);
            loggers::LXSC::DEBUG(
                "syscall args: a0=%p a1=%p a2=%p a3=%p a4=%p a5=%p a6=%p",
                reinterpret_cast<void *>(a0), reinterpret_cast<void *>(a1),
                reinterpret_cast<void *>(a2), reinterpret_cast<void *>(a3),
                reinterpret_cast<void *>(a4), reinterpret_cast<void *>(a5),
                reinterpret_cast<void *>(a6));
            // 直接退出
            return -ENOSYS;
    }
}
