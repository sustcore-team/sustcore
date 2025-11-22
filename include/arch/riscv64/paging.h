/**
 * @file paging.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISCV页表
 * @version alpha-1.0.0
 * @date 2025-11-22
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <arch/riscv64/mem/universal.h>

// SV39模式分页机制
#define SV39        1
// SV48模式分页机制
#define SV48        2
// 分页模式选择
#define PAGING_MODE SV39

#if PAGING_MODE == SV39

#include <arch/riscv64/mem/sv39.h>

// 相关类型设置
#define PTEntry   SV39PTE
#define PagingTab SV39PT

// 相关常量设置
#define ENTRIES_PER_PAGING_TAB SV39_PTE_COUNT

// 相关函数设置
#define mapping_init() sv39_mapping_init()
#define mem_root()     sv39_mapping_root()
#define mem_maps_to(root, vaddr, paddr, rwx, u, g) \
    sv39_maps_to(root, vaddr, paddr, rwx, u, g)
#define mem_maps_range_to(root, vstart, pstart, pages, rwx, u, g) \
    sv39_maps_range_to(root, vstart, pstart, pages, rwx, u, g)
#define mem_get_page(root, vaddr) sv39_get_pte(root, vaddr)

#define addr_v2p(root, vaddr) ppn2phyaddr(sv39_get_pte(root, vaddr)->ppn)

// 页面是否有效
#define PAGE_VALID(entry) ((entry != nullptr) && (entry)->v)

// 是否为用户页面
#define PAGE_U(entry) (PAGE_VALID(entry) && (entry)->u)

// 是否为全局页面
#define PAGE_G(entry) (PAGE_VALID(entry) && (entry)->g)

// 是否存在
#define PAGE_P(entry) (PAGE_VALID(entry) && (!(entry)->np))

// RWX位是否有效\
// rwx != 0 且 rwx.w => rwx.r
#define PAGE_RWX_VALID(entry)                    \
    (PAGE_VALID(entry) && ((entry)->rwx != 0) && \
     (BOOL_IMPLIES(((entry)->rwx) & RWX_MASK_W, ((entry)->rwx) & RWX_MASK_R)))

#define PAGE_RWX_R(entry) \
    ((PAGE_RWX_VALID(entry)) && (((entry)->rwx) & RWX_MASK_R))

#define PAGE_RWX_W(entry) \
    ((PAGE_RWX_VALID(entry)) && (((entry)->rwx) & RWX_MASK_W))

#define PAGE_RWX_X(entry) \
    ((PAGE_RWX_VALID(entry)) && (((entry)->rwx) & RWX_MASK_X))

#elif PAGING_MODE == SV48

#warning "SV48 unimplemented!"

#endif