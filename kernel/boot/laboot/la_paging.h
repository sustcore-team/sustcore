/**
 * @file la_paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief laboot 引导阶段分页辅助
 * @version alpha-1.0.0
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/laboot/macros.h>
#include <sus/types.h>
#include <sustcore/addr.h>

#define _LABOOT_PAGING SECTION(".laboot_paging")

namespace laboot {
    extern "C" char s_laboot, s_laboot_paging, e_laboot_paging, ekernel_phys;
    extern "C" [[noreturn]] void _laboot_post_start();
    extern "C" void _laboot_tlb_refill();

    constexpr size_t MINIMUM_PAGING_SIZE   = 128 * 1024;
    constexpr size_t MAXIMUM_KERNEL_SIZE   = 32 * 1024 * 1024;
    constexpr size_t MAXIMUM_DTB_SIZE      = 2 * 1024 * 1024;
    constexpr size_t PAGE_TABLE_ALIGNMENT  = 4 * 1024;
    constexpr size_t PAGING_ALIGNMENT      = 2 * 1024 * 1024;
    constexpr size_t PAGING_ALIGNMENT_MASK = PAGING_ALIGNMENT - 1;
    constexpr size_t PAGE_SIZE_1G          = 1024 * 1024 * 1024ULL;
    constexpr size_t PAGE_SIZE_1G_MASK     = PAGE_SIZE_1G - 1;

    constexpr size_t PAGE_SIZE    = 4096;
    constexpr size_t PAGE_SIZE_2M = 2 * 1024 * 1024;
    constexpr size_t PAGE_ENTRIES = 512;
    constexpr size_t PAGE_LEVELS  = 4;

    constexpr addr_t LABOOT_KVA_OFFSET = KVA_OFFSET;

    constexpr umb_t PAGE_PRESENT  = (1u << 7);
    constexpr umb_t PAGE_GLOBAL   = (1u << 6);
    constexpr umb_t PAGE_CACHE_CC = (1u << 4);
    constexpr umb_t PAGE_MODIFIED = (1u << 9);
    constexpr umb_t PAGE_WRITE    = (1u << 8);
    constexpr umb_t PAGE_DIRTY    = (1u << 1);
    constexpr umb_t PAGE_VALID    = (1u << 0);

    constexpr umb_t PTE_FLAGS = PAGE_PRESENT | PAGE_GLOBAL | PAGE_CACHE_CC |
                                PAGE_MODIFIED | PAGE_WRITE | PAGE_DIRTY |
                                PAGE_VALID;

    using pte_t = umb_t;

    constexpr pte_t PDE_BASE   = 0;
    constexpr pte_t PTE_BASE   = PTE_FLAGS;
    constexpr umb_t PPN_MASK   = 0x0000FFFFFFFFF000ULL;
    constexpr umb_t VPN_MASK   = 0x1FF;
    constexpr umb_t VPN3_SHIFT = 39;
    constexpr umb_t VPN2_SHIFT = 30;
    constexpr umb_t VPN1_SHIFT = 21;
    constexpr umb_t VPN0_SHIFT = 12;

    struct LabootPagingSetup {
        addr_t root_page_table;
        umb_t pwctl0;
        umb_t pwctl1;
        umb_t stlbpgsize;
        umb_t pgdl;
        umb_t pgdh;
        umb_t dmw0;
        umb_t dmw1;
        umb_t dmw2;
        umb_t dmw3;
        umb_t tlbrentry;
        umb_t crmd_value;
        addr_t post_entry;
    };

#define LA_TO_PPN(x)      (static_cast<umb_t>(x) >> 12)
#define LA_TO_PPNBASE(x)  (LA_TO_PPN(x) << 12)
#define LA_MAKE_PDE(addr) (LA_TO_PPNBASE(static_cast<umb_t>(addr)))
#define LA_MAKE_PTE(addr) (LA_TO_PPNBASE(static_cast<umb_t>(addr)) | PTE_BASE)
#define LA_TOVPN(vpn, vaddr)                         \
    do {                                             \
        auto __va = static_cast<umb_t>(vaddr);       \
        vpn[0]    = (__va >> VPN0_SHIFT) & VPN_MASK; \
        vpn[1]    = (__va >> VPN1_SHIFT) & VPN_MASK; \
        vpn[2]    = (__va >> VPN2_SHIFT) & VPN_MASK; \
        vpn[3]    = (__va >> VPN3_SHIFT) & VPN_MASK; \
    } while (0)

    inline void csr_write(umb_t reg, umb_t value) {
        asm volatile("csrwr %0, %1" ::"r"(value), "i"(reg) : "memory");
    }

    inline umb_t csr_read(umb_t reg) {
        umb_t value = 0;
        asm volatile("csrrd %0, %1" : "=r"(value) : "i"(reg) : "memory");
        return value;
    }

    inline void csr_set(umb_t reg, umb_t mask) {
        asm volatile("csrxchg %0, %0, %1" : "+r"(mask) : "i"(reg) : "memory");
    }

    inline void csr_clear(umb_t reg, umb_t mask) {
        umb_t zero = 0;
        asm volatile("csrxchg %0, %1, %2" : "+r"(zero) : "r"(mask), "i"(reg)
                     : "memory");
    }

    inline void tlb_flush_all() {
        asm volatile(
            "    dbar 0\n"
            "    invtlb 0x0, $zero, $zero\n"
            "    ibar 0" ::
                : "memory");
    }
}  // namespace laboot
