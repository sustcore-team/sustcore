/**
 * @file sv39.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SV39分页机制
 * @version alpha-1.0.0
 * @date 2025-11-20
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <arch/riscv64/mem/universal.h>
#include <stddef.h>
#include <sus/attributes.h>
#include <sus/bits.h>

/**
 * @brief SV39页表项
 *
 */
typedef union {
    umb_t value;
    struct {
        umb_t v : 1;     // [0] Valid 有效位
        umb_t rwx : 3;   // [1:3] RWX位
        umb_t u : 1;     // [4] User 用户态可访问位
        umb_t g : 1;     // [5] Global 全局页位
        umb_t a : 1;     // [6] Accessed 访问位
        umb_t d : 1;     // [7] Dirty 脏位
        umb_t rsw : 2;   // [8:9] Reserved for Software 软件保留位
        umb_t ppn : 44;  // [10:53] Physical Page Number 物理页号
        umb_t rsvd : 7;  // [54:60] 保留位
        umb_t pbmt : 2;  // [61:62] Page-Based Memory Type 基于页的内存类型
        umb_t np : 1;  // [63] Not Present 非存在位
    } PACKED;
} SV39PTE;

typedef struct {
    // 页表项
    SV39PTE *entry;
    // 大页级别(-1: 不存在 ,0: 4KB, 1: 2MB, 2: 1GB)
    int level;
} SV39LargablePTE;

/**
 * @brief 4K页大小
 *
 */
#define SV39_4K_PAGE_SIZE (4096)

/**
 * @brief 2M页大小
 *
 */
#define SV39_2M_PAGE_SIZE (2 * 1024 * 1024)

/**
 * @brief 1G页大小
 *
 */
#define SV39_1G_PAGE_SIZE (1024 * 1024 * 1024)

/**
 * @brief 默认页(小页)大小
 *
 */
#define SV39_PAGE_SIZE SV39_4K_PAGE_SIZE

/**
 * @brief SV39页表项数量
 *
 */
#define SV39_PTE_COUNT (SV39_PAGE_SIZE / sizeof(SV39PTE))

/**
 * @brief SV39页表
 *
 */
typedef SV39PTE *SV39PT;

/**
 * @brief 将物理页号转换为物理地址
 *
 * @param ppn 物理页号
 * @return void* 物理地址
 */
static inline void *ppn2phyaddr(umb_t ppn) {
    return (void *)(ppn << 12);
}

/**
 * @brief 将物理地址转换为物理页号
 *
 * @param phyaddr 物理地址
 * @return umb_t 物理页号
 */
static inline umb_t phyaddr2ppn(void *phyaddr) {
    return ((umb_t)phyaddr) >> 12;
}

/**
 * @brief 初始化SV39页表映射机制
 *
 */
void sv39_mapping_init(void);

/**
 * @brief 构造一个新的SV39根页表
 *
 * @return SV39PT SV39根页表
 */
SV39PT construct_sv39_mapping_root(void);

/**
 * @brief 获得SV39根页表
 *
 * @return SV39PT SV39根页表
 */
SV39PT sv39_mapping_root(void);

/**
 * @brief 在页表中映射虚拟地址到物理地址
 *
 * @param root 页表根指针
 * @param vaddr 虚拟地址
 * @param paddr 物理地址
 * @param rwx 读写执行权限
 * @param u 用户态可访问位
 * @param g 全局页位
 */
void sv39_maps_to(SV39PT root, void *vaddr, void *paddr, umb_t rwx, bool u,
                  bool g);

/**
 * @brief 在页表中映射虚拟地址到物理地址
 *
 * 其一次映射一个2MB的大页
 *
 * @param root 页表根指针
 * @param vaddr 虚拟地址
 * @param paddr 物理地址
 * @param rwx 读写执行权限
 * @param u 用户态可访问位
 * @param g 全局页位
 */
void sv39_maps_to_2m(SV39PT root, void *vaddr, void *paddr, umb_t rwx, bool u,
                     bool g);

/**
 * @brief 在页表中映射虚拟地址到物理地址
 *
 * 其一次映射一个1GB的大页
 *
 * @param root 页表根指针
 * @param vaddr 虚拟地址
 * @param paddr 物理地址
 * @param rwx 读写执行权限
 * @param u 用户态可访问位
 * @param g 全局页位
 */
void sv39_maps_to_1g(SV39PT root, void *vaddr, void *paddr, umb_t rwx, bool u,
                     bool g);

/**
 * @brief 在页表中映射一段虚拟地址到物理地址范围
 *
 * @param root 页表根指针
 * @param vstart 虚拟地址起始
 * @param pstart 物理地址起始
 * @param pages 映射大小(多少页)
 * @param rwx 读写执行权限
 * @param u 用户态可访问位
 * @param g 全局页位
 * @param pagewise 是否逐小页面映射(否则尝试使用大页映射)
 */
void sv39_maps_range_to(SV39PT root, void *vstart, void *pstart, size_t pages,
                        umb_t rwx, bool u, bool g, bool pagewise);

/**
 * @brief 获得页面项
 *
 * @param root 页表根
 * @param vaddr 虚拟地址
 * @return SV39LargablePTE* 页面项
 */
SV39LargablePTE sv39_get_pte(SV39PT root, void *vaddr);
