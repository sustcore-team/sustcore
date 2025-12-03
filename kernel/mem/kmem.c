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

#include <basec/logger.h>
#include <mem/kmem.h>
#include <sus/paging.h>
#include <sus/symbols.h>

bool post_init_flag = false;
dword phymem_sz     = 0;
void *upper_bound   = nullptr;

void create_kernel_paging(void *root) {
    // 把内核部分映射到高地址
    umb_t kernel_pages =
        ((umb_t)&ekernel - (umb_t)&skernel + PAGE_SIZE - 1) / PAGE_SIZE;

    log_info("内核地址偏移:      %p", (umb_t)KERNEL_VA_OFFSET);
    void *kernel_vaddr_start =
        (void *)(((umb_t)&skernel) + (umb_t)KERNEL_VA_OFFSET);
    log_info("内核虚拟地址空间: [%p, %p)", kernel_vaddr_start,
             (void *)((umb_t)kernel_vaddr_start + kernel_pages * PAGE_SIZE));

    // 首先映射代码段
    void *text_vaddr_start =
        (void *)(((umb_t)&s_text) + (umb_t)KERNEL_VA_OFFSET);
    umb_t text_pages =
        ((umb_t)&e_text - (umb_t)&s_text + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核代码段虚拟地址空间: [%p, %p)", text_vaddr_start,
             (void *)((umb_t)text_vaddr_start + text_pages * PAGE_SIZE));
    mem_maps_range_to(root, text_vaddr_start, (void *)&s_text, text_pages,
                      RWX_MODE_RX, false, true);

    // 接着映射ivt
    void *ivt_vaddr_start = (void *)(((umb_t)&s_ivt) + (umb_t)KERNEL_VA_OFFSET);
    umb_t ivt_pages =
        ((umb_t)&e_ivt - (umb_t)&s_ivt + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核IVT段虚拟地址空间: [%p, %p)", ivt_vaddr_start,
             (void *)((umb_t)ivt_vaddr_start + ivt_pages * PAGE_SIZE));
    mem_maps_range_to(root, ivt_vaddr_start, (void *)&s_ivt, ivt_pages,
                      RWX_MODE_RWX, false, true);

    // 再映射只读数据段
    void *rodata_vaddr_start =
        (void *)(((umb_t)&s_rodata) + (umb_t)KERNEL_VA_OFFSET);
    umb_t rodata_pages =
        ((umb_t)&e_rodata - (umb_t)&s_rodata + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核只读数据段虚拟地址空间: [%p, %p)", rodata_vaddr_start,
             (void *)((umb_t)rodata_vaddr_start + rodata_pages * PAGE_SIZE));
    mem_maps_range_to(root, rodata_vaddr_start, (void *)&s_rodata, rodata_pages,
                      RWX_MODE_R, false, true);

    // 最后映射数据段, 初始数据段与BSS段
    void *data_vaddr_start =
        (void *)(((umb_t)&s_data) + (umb_t)KERNEL_VA_OFFSET);
    umb_t data_pages =
        ((umb_t)&e_data - (umb_t)&s_data + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核数据段虚拟地址空间: [%p, %p)", data_vaddr_start,
             (void *)((umb_t)data_vaddr_start + data_pages * PAGE_SIZE));
    mem_maps_range_to(root, data_vaddr_start, (void *)&s_data, data_pages,
                      RWX_MODE_RW, false, true);

    // BSS段
    void *bss_vaddr_start = (void *)(((umb_t)&s_bss) + (umb_t)KERNEL_VA_OFFSET);
    umb_t bss_pages =
        ((umb_t)&e_bss - (umb_t)&s_bss + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核BSS段虚拟地址空间: [%p, %p)", bss_vaddr_start,
             (void *)((umb_t)bss_vaddr_start + bss_pages * PAGE_SIZE));
    mem_maps_range_to(root, bss_vaddr_start, (void *)&s_bss, bss_pages,
                      RWX_MODE_RW, false, true);

    // 剩余部分
    void *misc_vaddr_start =
        (void *)(((umb_t)&s_misc) + (umb_t)KERNEL_VA_OFFSET);
    umb_t misc_pages =
        ((umb_t)&ekernel - (umb_t)&s_misc + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核剩余部分虚拟地址空间: [%p, %p)", misc_vaddr_start,
             (void *)((umb_t)misc_vaddr_start + misc_pages * PAGE_SIZE));
    mem_maps_range_to(root, misc_vaddr_start, (void *)&s_misc, misc_pages,
                      RWX_MODE_RX, false, true);

    // 内核物理地址空间映射
    void *kphy_vaddr_start = (void *)(((umb_t)0x0) + (umb_t)KPHY_VA_OFFSET);
    umb_t kphy_pages       = ((umb_t)upper_bound + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核物理地址空间映射: [%p, %p)", kphy_vaddr_start,
             (void *)((umb_t)kphy_vaddr_start + kphy_pages * PAGE_SIZE));
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
    phymem_sz = (umb_t)upper_bound;
    log_info("内存地址上界:   %p", upper_bound);
}