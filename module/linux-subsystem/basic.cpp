/**
 * @file basic.cpp
 * @author theflysong
 * @brief linux subsystem 中用户程序基础运行时逻辑
 * @version alpha-1.0.0
 * @date 2026-06-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#include <prm.h>
#include <prog.h>
#include <syscall.h>

#include <cstdio>
#include <cstring>

namespace {
    constexpr size_t INVALID_VALUE      = 0xFFFF'FFFF'FFFF'FFFF;
    constexpr size_t UTSNAME_FIELD_SIZE = 65;

    struct linux_utsname {
        char sysname[UTSNAME_FIELD_SIZE];
        char nodename[UTSNAME_FIELD_SIZE];
        char release[UTSNAME_FIELD_SIZE];
        char version[UTSNAME_FIELD_SIZE];
        char machine[UTSNAME_FIELD_SIZE];
        char domainname[UTSNAME_FIELD_SIZE];
    };

    bool parse_tagged_index(const char *text, const char *prefix,
                            size_t &value) {
        if (text == nullptr || prefix == nullptr) {
            return false;
        }
        size_t prefix_len = strlen(prefix);
        if (strncmp(text, prefix, prefix_len) != 0) {
            return false;
        }
        value = 0;
        for (const char *p = text + prefix_len; *p != '\0'; ++p) {
            if (*p < '0' || *p > '9') {
                return false;
            }
            value = value * 10 + static_cast<size_t>(*p - '0');
        }
        return true;
    }

    bool has_memory_kind(const char *desc, const char *kind) {
        if (desc == nullptr || kind == nullptr || desc[0] != '#') {
            return false;
        }
        ++desc;
        size_t kind_len = strlen(kind);
        return strncmp(desc, kind, kind_len) == 0 && desc[kind_len] == ':';
    }

    void copy_uts_field(char (&dst)[UTSNAME_FIELD_SIZE],
                        const char *src) noexcept {
        if (src == nullptr) {
            dst[0] = '\0';
            return;
        }
        strncpy(dst, src, UTSNAME_FIELD_SIZE - 1);
        dst[UTSNAME_FIELD_SIZE - 1] = '\0';
    }

    const char *linux_machine_name() noexcept {
#if defined(__ARCH_riscv64__)
        return "riscv64";
#elif defined(__ARCH_loongarch64__)
        return "loongarch64";
#else
        return "unknown";
#endif
    }
}  // namespace

size_t __prog_heap_base    = 0;
size_t __prog_brk          = 0;
CapIdx __prog_pcb_cap      = cap::null;
CapIdx __prog_main_tcb_cap = cap::null;
CapIdx __prog_heap_mem_cap = cap::null;
CapIdx __prog_root_dir_cap  = cap::null;

void init_prog_data(size_t bsargc, const bsheader *bsargv[]) {
    __prog_heap_base    = 0;
    __prog_brk          = 0;
    __prog_pcb_cap      = cap::null;
    __prog_main_tcb_cap = cap::null;
    __prog_heap_mem_cap = cap::null;
    __prog_root_dir_cap  = cap::null;

    for (size_t i = 0; i < bsargc; ++i) {
        BootstrapRecordView view{};
        if (!bootstrap_make_view(bsargv[i], view)) {
            continue;
        }

        if (view.header->type == boot::TYPE_CAPEXP) {
            BootstrapCapExplainView cap_view{};
            if (!bootstrap_parse_cap_explain(view, cap_view)) {
                continue;
            }

            size_t tagged_idx = 0;
            if (cap_view.cap_type == PayloadType::PCB &&
                parse_tagged_index(cap_view.cap_desc, "#self:", tagged_idx))
            {
                __prog_pcb_cap = cap_view.cap_idx;
                continue;
            }
            if (cap_view.cap_type == PayloadType::TCB &&
                parse_tagged_index(cap_view.cap_desc, "#main:", tagged_idx))
            {
                __prog_main_tcb_cap = cap_view.cap_idx;
                continue;
            }
            if (cap_view.cap_type == PayloadType::MEMORY &&
                has_memory_kind(cap_view.cap_desc, "heap"))
            {
                __prog_heap_mem_cap = cap_view.cap_idx;
                continue;
            }
            if (cap_view.cap_type == PayloadType::VDIR &&
                cap_view.cap_desc != nullptr &&
                cap_view.cap_desc[0] == '#')
            {
                if (strcmp(cap_view.cap_desc + 1, "/") == 0) {
                    __prog_root_dir_cap = cap_view.cap_idx;
                }
                continue;
            }
            continue;
        }

        if (view.header->type == boot::TYPE_VADDREXP) {
            BootstrapVaddrExplainView vaddr_view{};
            if (!bootstrap_parse_vaddr_explain(view, vaddr_view)) {
                continue;
            }
            if (strcmp(vaddr_view.vaddr_desc, "#heap") == 0) {
                __prog_heap_base = vaddr_view.vaddr.arith();
                __prog_brk       = vaddr_view.vaddr.arith();
            }
        }
    }
}

size_t linux_sys_brk(size_t newbrk) {
    if (newbrk == 0) {
        return __prog_brk;
    }

    if (newbrk < __prog_heap_base) {
        return __prog_brk;
    }
    if (__prog_heap_mem_cap == cap::null) {
        return __prog_brk;
    }
    if (!sys_mem_resize(__prog_heap_mem_cap, newbrk - __prog_heap_base)) {
        return __prog_brk;
    }

    __prog_brk = newbrk;
    return __prog_brk;
}

size_t linux_sys_uname(void *buf) {
    if (buf == nullptr) {
        return INVALID_VALUE;
    }

    linux_utsname uts{};
    copy_uts_field(uts.sysname, "sustcore");
    copy_uts_field(uts.nodename, "qemu");
    copy_uts_field(uts.release, "4.15.0");
    copy_uts_field(uts.version, "build 0");
    copy_uts_field(uts.machine, linux_machine_name());
    copy_uts_field(uts.domainname, "(none)");
    memcpy(buf, &uts, sizeof(uts));
    return 0;
}
[[noreturn]]
void linux_sys_exit(int exitcode) {
    sys_pcb_kill(__prog_pcb_cap, exitcode);
    loggers::LXRT::ERROR("sys_exit 返回到不应执行的位置: exitcode=%d",
                         exitcode);
    while (true);
}
