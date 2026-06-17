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

#include <arch/loongarch64/mem/paging.h>
#include <sus/types.h>
#include <sustcore/addr.h>

#define _LABOOT_RECLAIMABLE SECTION(".laboot_reclaimable")

#define LABOOT_PTE_IS_VALID(x) (((x) & LA_PAGE_VALID) != 0)
#define LABOOT_PTE_IS_LEAF(x)  (((x) & LA_PTE_FLAGS) != 0)
#define LABOOT_PTE_TO_PA(x)    ((x) & LA_PPN_MASK)

namespace laboot {
    extern "C" char s_laboot, s_laboot_kva, s_laboot_reclaimable,
        s_laboot_reclaimable_kva, e_laboot_reclaimable,
        e_laboot_reclaimable_kva, ekernel_phys, ekernel;
    extern "C" void _laboot_tlb_refill();
    extern "C" addr_t __laboot_bsp_phys_id;
    extern "C" addr_t __laboot_cmdline_phys;
    extern "C" addr_t __laboot_system_table_phys;

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

    constexpr umb_t PAGE_PRESENT  = LA_PAGE_PRESENT;
    constexpr umb_t PAGE_GLOBAL   = LA_PAGE_GLOBAL;
    constexpr umb_t PAGE_CACHE_CC = LA_PAGE_CACHE_CC;
    constexpr umb_t PAGE_MODIFIED = LA_PAGE_MODIFIED;
    constexpr umb_t PAGE_WRITE    = LA_PAGE_WRITE;
    constexpr umb_t PAGE_DIRTY    = LA_PAGE_DIRTY;
    constexpr umb_t PAGE_VALID    = LA_PAGE_VALID;

    constexpr umb_t PTE_FLAGS = LA_PTE_FLAGS;

    using pte_t = umb_t;

    constexpr pte_t PDE_BASE   = 0;
    constexpr pte_t PTE_BASE   = PTE_FLAGS;
    constexpr umb_t PPN_MASK   = LA_PPN_MASK;
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
        addr_t boot_info_ptr;
        addr_t reclaimable_cursor;
        addr_t reserved;
    };

    struct LabootInfo {
        uint64_t bsp_phys_id;
        uint64_t dtb_phys;
        uint64_t dtb_virt;
        uint64_t hhdm_base;
        uint64_t kernel_phys_base;
        uint64_t kernel_virt_base;
        uint64_t kernel_phys_end;
        uint64_t kernel_virt_end;
        uint64_t root_page_table_phys;
        uint64_t root_page_table_virt;
        uint64_t cmdline_phys;
        uint64_t system_table_phys;
        uint64_t cmdline_virt;
        uint64_t system_table_virt;
    };

#define LA_TO_PPN(x)      (static_cast<umb_t>(x) >> 12)
#define LA_TO_PPNBASE(x)  (LA_TO_PPN(x) << 12)
#define LA_MAKE_PDE(addr) (LA_TO_PPNBASE(static_cast<umb_t>(addr)))
#define LA_MAKE_PTE(addr) (LA_TO_PPNBASE(static_cast<umb_t>(addr)) | PTE_BASE)
#define LA_PTE_IS_VALID(entry) LABOOT_PTE_IS_VALID(entry)
#define LA_PTE_IS_LEAF(entry)  LABOOT_PTE_IS_LEAF(entry)
#define LA_PTE_TO_PA(entry)    LABOOT_PTE_TO_PA(entry)
#define LA_TOVPN(vpn, vaddr)                         \
    do {                                             \
        auto __va = static_cast<umb_t>(vaddr);       \
        vpn[0]    = (__va >> VPN0_SHIFT) & VPN_MASK; \
        vpn[1]    = (__va >> VPN1_SHIFT) & VPN_MASK; \
        vpn[2]    = (__va >> VPN2_SHIFT) & VPN_MASK; \
        vpn[3]    = (__va >> VPN3_SHIFT) & VPN_MASK; \
    } while (0)
}  // namespace laboot
