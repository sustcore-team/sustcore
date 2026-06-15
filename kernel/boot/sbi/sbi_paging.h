/**
 * @file sbi_paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief sbi 引导分页相关代码
 * @version alpha-1.0.0
 * @date 2026-06-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sustcore/addr.h>

#define _SBI_RECLAIMABLE SECTION(".sbi_reclaimable")

namespace sbi {
    extern "C" char s_sbi, s_sbi_kva, s_sbi_reclaimable,
        s_sbi_reclaimable_kva, e_sbi_reclaimable, e_sbi_reclaimable_kva,
        ekernel_phys, ekernel;
    constexpr size_t MINIMUM_PAGING_SIZE   = 128 * 1024;         // 64KB
    constexpr size_t MAXIMUM_KERNEL_SIZE   = 32 * 1024 * 1024;  // 32MB
    constexpr size_t MAXIMUM_DTB_SIZE      = 2 * 1024 * 1024;   // 2MB
    constexpr size_t PAGE_TABLE_ALIGNMENT  = 4 * 1024;          // 4KB
    constexpr size_t PAGING_ALIGNMENT      = 2 * 1024 * 1024;   // 2MB
    constexpr size_t PAGING_ALIGNMENT_MASK = 0x1FFFFF;          // 2MB - 1
    constexpr size_t PAGE_SIZE_1G          = 1024 * 1024 * 1024ULL;
    constexpr size_t PAGE_SIZE_1G_MASK     = PAGE_SIZE_1G - 1;

    using pte_t = size_t;  // 页目录项类型

    constexpr pte_t PDE_BASE  = 0x1;
    constexpr pte_t PTE_BASE  = 0xF;
    constexpr size_t PPN_MASK = 0x3FFFFFFFFFFC00ULL;  // 44位物理页号掩码
    constexpr size_t PPN_SHIFT = 10;  // PPN在PTE中的起始位

    constexpr size_t VPN_MASK               = 0x1FF;  // 9位虚拟页号掩码
    constexpr size_t VPN_INPAGE_OFFSET_MASK = 0x3FF;  // 页内偏移掩码
    constexpr size_t VPN2_SHIFT = 30;  // VPN2在虚拟地址中的起始位
    constexpr size_t VPN1_SHIFT = 21;  // VPN1在虚拟地址中的起始位
    constexpr size_t VPN0_SHIFT = 12;  // VPN0在虚拟地址中的起始位

    constexpr size_t PAGE_SIZE    = 4096;             // 4KB
    constexpr size_t PAGE_SIZE_2M = 2 * 1024 * 1024;  // 2MB
    constexpr size_t PAGE_ENTRIES = 512;  // 每页目录/页表项数

    constexpr size_t SATP_SV39_BASE = 0x8000000000000000ULL;  // SV39模式标志位

    #define TO_PPN(x) ((size_t)(x) >> 12) // 将地址转换为物理页号
    #define TO_PPNBASE(x) ((TO_PPN(x) << PPN_SHIFT) & PPN_MASK)
    #define MAKE_PDE(peaddr) (TO_PPNBASE((size_t)(peaddr)) | PDE_BASE)
    #define MAKE_PTE(pfaddr) (TO_PPNBASE((size_t)(pfaddr)) | PTE_BASE)
    #define TOVPN(vpn, vaddr) \
        do { \
            auto vaddr_num = (size_t)(vaddr); \
            vpn[0]         = (vaddr_num >> VPN0_SHIFT) & VPN_MASK; \
            vpn[1]         = (vaddr_num >> VPN1_SHIFT) & VPN_MASK; \
            vpn[2]         = (vaddr_num >> VPN2_SHIFT) & VPN_MASK; \
        } while (0)
}  // namespace sbi
