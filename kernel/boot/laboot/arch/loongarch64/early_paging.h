/**
 * @file early_paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief LABOOT LoongArch 早期分页辅助
 * @version 0.1.0-dev.1
 * @date 2026-06-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/loongarch64/pagedef.h>
#include <sustcore/addr.h>
#include <tay/attribute.h>
#include <tay/bits.h>

#define _LABOOT_RECLAIMABLE SECTION(".laboot_reclaimable")

#define LABOOT_PTE_IS_VALID(x) (((x) & LA_PAGE_VALID) != 0)
#define LABOOT_PTE_IS_LEAF(x)  (((x) & LA_PTE_FLAGS) != 0)
#define LABOOT_PTE_TO_PA(x)    ((x) & LA_PPN_MASK)

#define _LABOOT_TEXT        SECTION(".laboot.text")
#define _LABOOT_RODATA      SECTION(".laboot.rodata")
#define _LABOOT_DATA        SECTION(".laboot.data")
#define _LABOOT_BSS         SECTION(".laboot.bss")
#define _LABOOT_POST_TEXT   SECTION(".laboot_post.text")
#define _LABOOT_POST_RODATA SECTION(".laboot_post.rodata")
#define _LABOOT_POST_DATA   SECTION(".laboot_post.data")
#define _LABOOT_POST_BSS    SECTION(".laboot_post.bss")

namespace laboot {
    extern "C" char s_laboot, s_laboot_kva, s_laboot_reclaimable, s_laboot_reclaimable_kva,
        e_laboot_reclaimable, e_laboot_reclaimable_kva, ekernel_phys, ekernel, s_init, e_init;
    extern "C" _LABOOT_TEXT void _laboot_tlb_refill();
    extern "C" _LABOOT_BSS addr_t __laboot_bsp_phys_id;
    extern "C" _LABOOT_BSS addr_t __laboot_cmdline_phys;
    extern "C" _LABOOT_BSS addr_t __laboot_system_table_phys;

    constexpr size_t MINIMUM_PAGING_SIZE   = 128 * 1024;
    constexpr size_t MAXIMUM_KERNEL_SIZE   = 32 * 1024 * 1024;
    constexpr size_t MAXIMUM_DTB_SIZE      = 2 * 1024 * 1024;
    constexpr size_t PAGE_TABLE_ALIGNMENT  = 4 * 1024;
    constexpr size_t PAGING_ALIGNMENT      = 2 * 1024 * 1024;
    constexpr size_t PAGING_ALIGNMENT_MASK = PAGING_ALIGNMENT - 1;
    constexpr size_t PAGE_SIZE_1G          = 1024 * 1024 * 1024ULL;
    constexpr size_t PAGE_SIZE_1G_MASK     = PAGE_SIZE_1G - 1;

    constexpr size_t PAGE_SIZE_2M = 2 * 1024 * 1024;
    constexpr size_t PAGE_ENTRIES = 512;
    constexpr size_t PAGE_LEVELS  = 4;

    constexpr addr_t LABOOT_KVA_START = KVA_START;

    constexpr xlen_t PAGE_PRESENT  = LA_PAGE_PRESENT;
    constexpr xlen_t PAGE_GLOBAL   = LA_PAGE_GLOBAL;
    constexpr xlen_t PAGE_CACHE_CC = LA_PAGE_CACHE_CC;
    constexpr xlen_t PAGE_MODIFIED = LA_PAGE_MODIFIED;
    constexpr xlen_t PAGE_WRITE    = LA_PAGE_WRITE;
    constexpr xlen_t PAGE_DIRTY    = LA_PAGE_DIRTY;
    constexpr xlen_t PAGE_VALID    = LA_PAGE_VALID;

    constexpr xlen_t PTE_FLAGS = LA_PTE_FLAGS;

    using PteType = xlen_t;

    constexpr PteType PDE_BASE  = 0;
    constexpr PteType PTE_BASE  = PTE_FLAGS;
    constexpr xlen_t PPN_MASK   = LA_PPN_MASK;
    constexpr xlen_t VPN_MASK   = 0x1FF;
    constexpr xlen_t VPN3_SHIFT = 39;
    constexpr xlen_t VPN2_SHIFT = 30;
    constexpr xlen_t VPN1_SHIFT = 21;
    constexpr xlen_t VPN0_SHIFT = 12;

    struct LabootPagingSetup {
        addr_t root_page_table;
        xlen_t pwctl0;
        xlen_t pwctl1;
        xlen_t stlbpgsize;
        xlen_t pgdl;
        xlen_t pgdh;
        xlen_t dmw0;
        xlen_t dmw1;
        xlen_t dmw2;
        xlen_t dmw3;
        xlen_t tlbrentry;
        xlen_t crmd_value;
        addr_t post_entry;
        addr_t boot_info_ptr;
        addr_t reclaimable_cursor;
        addr_t reserved;
    };

    struct LabootInfo {
        u64_t bsp_phys_id;
        u64_t dtb_phys;
        u64_t dtb_virt;
        u64_t hhdm_base;
        u64_t kernel_phys_base;
        u64_t kernel_virt_base;
        u64_t kernel_phys_end;
        u64_t kernel_virt_end;
        u64_t root_page_table_phys;
        u64_t root_page_table_virt;
        u64_t cmdline_phys;
        u64_t system_table_phys;
        u64_t cmdline_virt;
        u64_t system_table_virt;
    };

#define LA_TO_PPN(x)           (static_cast<xlen_t>(x) >> 12)
#define LA_TO_PPNBASE(x)       (LA_TO_PPN(x) << 12)
#define LA_MAKE_PDE(addr)      (LA_TO_PPNBASE(static_cast<xlen_t>(addr)))
#define LA_MAKE_PTE(addr)      (LA_TO_PPNBASE(static_cast<xlen_t>(addr)) | PTE_BASE)
#define LA_PTE_IS_VALID(entry) LABOOT_PTE_IS_VALID(entry)
#define LA_PTE_IS_LEAF(entry)  LABOOT_PTE_IS_LEAF(entry)
#define LA_PTE_TO_PA(entry)    LABOOT_PTE_TO_PA(entry)
#define LA_TOVPN(vpn, vaddr)                         \
    do {                                             \
        auto __va = static_cast<xlen_t>(vaddr);      \
        vpn[0]    = (__va >> VPN0_SHIFT) & VPN_MASK; \
        vpn[1]    = (__va >> VPN1_SHIFT) & VPN_MASK; \
        vpn[2]    = (__va >> VPN2_SHIFT) & VPN_MASK; \
        vpn[3]    = (__va >> VPN3_SHIFT) & VPN_MASK; \
    } while (0)
}  // namespace laboot
