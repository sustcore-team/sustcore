/**
 * @file kmem.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核内存相关
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <mem/kmem.h>
#include <sus/paging.h>
#include <sus/symbols.h>
#include <basec/logger.h>

bool post_init_flag = false;
size_t phymem_sz     = 0;
void *upper_bound   = nullptr;

void *skernel_paddr = nullptr, *ekernel_paddr = nullptr;
void *stext_paddr = nullptr, *etext_paddr = nullptr;
void *sivt_paddr = nullptr, *eivt_paddr = nullptr;
void *srodata_paddr = nullptr, *erodata_paddr = nullptr;
void *sdata_paddr = nullptr, *edata_paddr = nullptr;
void *sbss_paddr = nullptr, *ebss_paddr = nullptr;
void *smisc_paddr = nullptr;

void create_kernel_paging(void *root) {
    // 把内核部分映射到高地址
    size_t kernelsz    = ekernel_paddr - skernel_paddr;
    umb_t kernel_pages = (kernelsz + PAGE_SIZE - 1) / PAGE_SIZE;

    log_info("内核地址偏移:      %p", (umb_t)KERNEL_VA_OFFSET);
    void *kernel_vaddr_start = skernel_paddr + KERNEL_VA_OFFSET;
    log_info("内核虚拟地址空间: [%p, %p)", kernel_vaddr_start,
             kernel_vaddr_start + kernel_pages * PAGE_SIZE);

    // 首先映射代码段
    void *text_vaddr_start = stext_paddr + KERNEL_VA_OFFSET;
    size_t textsz          = etext_paddr - stext_paddr;
    size_t text_pages      = (textsz + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核代码段虚拟地址空间: [%p, %p)", text_vaddr_start,
             text_vaddr_start + text_pages * PAGE_SIZE);
    mem_maps_range_to(root, text_vaddr_start, stext_paddr, text_pages,
                      RWX_MODE_RX, false, true);

    // 接着映射ivt
    void *ivt_vaddr_start = sivt_paddr + KERNEL_VA_OFFSET;
    size_t ivtsz          = eivt_paddr - sivt_paddr;
    size_t ivt_pages      = (ivtsz + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核IVT段虚拟地址空间: [%p, %p)", ivt_vaddr_start,
             (void *)((umb_t)ivt_vaddr_start + ivt_pages * PAGE_SIZE));
    mem_maps_range_to(root, ivt_vaddr_start, sivt_paddr, ivt_pages, RWX_MODE_RWX,
                      false, true);

    // 再映射只读数据段
    void *rodata_vaddr_start = srodata_paddr + (umb_t)KERNEL_VA_OFFSET;
    size_t rodata_sz         = erodata_paddr - srodata_paddr;
    size_t rodata_pages      = (rodata_sz + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核只读数据段虚拟地址空间: [%p, %p)", rodata_vaddr_start,
             rodata_vaddr_start + rodata_pages * PAGE_SIZE);
    mem_maps_range_to(root, rodata_vaddr_start, srodata_paddr, rodata_pages,
                      RWX_MODE_R, false, true);

    // 最后映射数据段, 初始数据段与BSS段
    void *data_vaddr_start = sdata_paddr + (umb_t)KERNEL_VA_OFFSET;
    size_t data_sz         = edata_paddr - sdata_paddr;
    size_t data_pages      = (data_sz + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核数据段虚拟地址空间: [%p, %p)", data_vaddr_start,
             data_vaddr_start + data_pages * PAGE_SIZE);
    mem_maps_range_to(root, data_vaddr_start, sdata_paddr, data_pages,
                      RWX_MODE_RW, false, true);

    // BSS段
    void *bss_vaddr_start = sbss_paddr + (umb_t)KERNEL_VA_OFFSET;
    size_t bss_sz         = ebss_paddr - sbss_paddr;
    size_t bss_pages      = (bss_sz + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核BSS段虚拟地址空间: [%p, %p)", bss_vaddr_start,
             bss_vaddr_start + bss_pages * PAGE_SIZE);
    mem_maps_range_to(root, bss_vaddr_start, sbss_paddr, bss_pages, RWX_MODE_RW,
                      false, true);

    // 剩余部分
    void *misc_vaddr_start = smisc_paddr + (umb_t)KERNEL_VA_OFFSET;
    size_t misc_sz         = ekernel_paddr - smisc_paddr;
    size_t misc_pages      = (misc_sz + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核剩余部分虚拟地址空间: [%p, %p)", misc_vaddr_start,
             misc_vaddr_start + misc_pages * PAGE_SIZE);
    mem_maps_range_to(root, misc_vaddr_start, smisc_paddr, misc_pages,
                      RWX_MODE_RX, false, true);

    // 内核物理地址空间映射
    void *kphy_vaddr_start = (void *)(((umb_t)0x0) + (umb_t)KPHY_VA_OFFSET);
    size_t kphy_pages       = ((umb_t)upper_bound + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核物理地址空间映射: [%p, %p)", kphy_vaddr_start,
             kphy_vaddr_start + kphy_pages * PAGE_SIZE);
    mem_maps_range_to(root, kphy_vaddr_start, (void *)0x0, kphy_pages,
                      RWX_MODE_RWX, false, true);
}

void setup_kernel_paging(MemRegion *const layout) {
    log_info("初始化分页机制...");
    mapping_init();
    log_info("分页机制初始化完成!");

    // 根据layout找到地址上界upper_bound
    upper_bound     = nullptr;
    MemRegion *iter = layout;
    while (iter != nullptr) {
        void *end_addr =
            (void *)(((umb_t)iter->addr + iter->size + PAGE_SIZE - 1) &
                     ~(PAGE_SIZE - 1));
        if (end_addr > upper_bound) {
            upper_bound = end_addr;
        }
        iter = iter->next;
    }

    // 将此作为物理内存大小
    phymem_sz = (size_t)upper_bound;
    log_info("内存地址上界:   %p", upper_bound);

    // 计算skernel_paddr与ekernel_paddr等地址
    skernel_paddr = (void *)&skernel;
    ekernel_paddr = (void *)&ekernel;
    stext_paddr   = (void *)&s_text;
    etext_paddr   = (void *)&e_text;
    sivt_paddr    = (void *)&s_ivt;
    eivt_paddr    = (void *)&e_ivt;
    srodata_paddr = (void *)&s_rodata;
    erodata_paddr = (void *)&e_rodata;
    sdata_paddr   = (void *)&s_data;
    edata_paddr   = (void *)&e_data;
    sbss_paddr    = (void *)&s_bss;
    ebss_paddr    = (void *)&e_bss;
    smisc_paddr   = (void *)&s_misc;
}