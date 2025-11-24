/**
 * @file main.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <basec/logger.h>
#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/pmm.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sus/arch.h>
#include <sus/boot.h>
#include <sus/symbols.h>

/**
 * @brief 内核主函数
 *
 * @return int
 */
int main(void) {
    // 读取attach.license段
    size_t license_size =
        (size_t)((umb_t)&e_attach_license - (umb_t)&s_attach_license);
    license_size = license_size > 2048 ?
            2048 :
            license_size;
    char *license_str = (char *)kmalloc(license_size + 1);
    memcpy(license_str, &s_attach_license, license_size);
    license_str[license_size] = '\0';
    log_info("内核附加文件: license");
    log_info("----- BEGIN LICENSE -----");
    kprintf("%s\n", license_str);
    log_info("-----  END LICENSE  -----");
    kfree(license_str);

    log_info("Hello RISCV World!");

    while (true);

    return 0;
}

/**
 * @brief 收尾工作
 *
 */
void terminate(void) {
    arch_terminate();
    log_info("内核已关闭!");
    while (true);
}

void post_init(void);

umb_t phymem_sz = 0;

/**
 * @brief 内核页表设置
 *
 */
void kernel_paging_setup(MemRegion *const layout) {
    log_info("初始化分页机制...");
    mapping_init();
    log_info("分页机制初始化完成!");

    // 首先构造一个根页表
    PagingTab root = mem_construct_root();
    log_info("构造根页表完成: %p", root);

    // 根据layout找到地址上界upper_bound
    void *upper_bound = nullptr;
    MemRegion *iter   = layout;
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

    kfree(layout);

    log_info("内存地址上界:   %p", upper_bound);
    // 对[0, upper_bound)作恒等映射
    mem_maps_range_to(root, (void *)0x0, (void *)0x0, phymem_sz / PAGE_SIZE,
                      RWX_MODE_RWX, false, true);

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
    // 初始数据段
    void *sdata_vaddr_start =
        (void *)(((umb_t)&s_sdata) + (umb_t)KERNEL_VA_OFFSET);
    umb_t sdata_pages =
        ((umb_t)&e_sdata - (umb_t)&s_sdata + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核初始化数据段虚拟地址空间: [%p, %p)", sdata_vaddr_start,
             (void *)((umb_t)sdata_vaddr_start + sdata_pages * PAGE_SIZE));
    mem_maps_range_to(root, sdata_vaddr_start, (void *)&s_sdata, sdata_pages,
                      RWX_MODE_RW, false, true);

    // BSS段
    void *bss_vaddr_start = (void *)(((umb_t)&s_bss) + (umb_t)KERNEL_VA_OFFSET);
    umb_t bss_pages =
        ((umb_t)&e_bss - (umb_t)&s_bss + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核BSS段虚拟地址空间: [%p, %p)", bss_vaddr_start,
             (void *)((umb_t)bss_vaddr_start + bss_pages * PAGE_SIZE));
    mem_maps_range_to(root, bss_vaddr_start, (void *)&s_bss, bss_pages,
                      RWX_MODE_RW, false, true);

    // SBSS段
    void *sbss_vaddr_start =
        (void *)(((umb_t)&s_sbss) + (umb_t)KERNEL_VA_OFFSET);
    umb_t sbss_pages =
        ((umb_t)&e_sbss - (umb_t)&s_sbss + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核SBSS段虚拟地址空间: [%p, %p)", sbss_vaddr_start,
             (void *)((umb_t)sbss_vaddr_start + sbss_pages * PAGE_SIZE));
    mem_maps_range_to(root, sbss_vaddr_start, (void *)&s_sbss, sbss_pages,
                      RWX_MODE_RW, false, true);

    // 剩余部分
    void *misc_vaddr_start =
        (void *)(((umb_t)&s_misc) + (umb_t)KERNEL_VA_OFFSET);
    umb_t misc_pages =
        ((umb_t)&ekernel - (umb_t)&s_misc + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核剩余部分虚拟地址空间: [%p, %p)", misc_vaddr_start,
             (void *)((umb_t)misc_vaddr_start + misc_pages * PAGE_SIZE));
    mem_maps_range_to(root, misc_vaddr_start, (void *)&s_misc, misc_pages,
                      RWX_MODE_R, false, true);

    // 内核物理地址空间映射
    void *kphy_vaddr_start = (void *)(((umb_t)0x0) + (umb_t)KPHY_VA_OFFSET);
    umb_t kphy_pages       = ((umb_t)upper_bound + PAGE_SIZE - 1) / PAGE_SIZE;
    log_info("内核物理地址空间映射: [%p, %p)", kphy_vaddr_start,
             (void *)((umb_t)kphy_vaddr_start + kphy_pages * PAGE_SIZE));
    mem_maps_range_to(root, kphy_vaddr_start, (void *)0x0, kphy_pages,
                      RWX_MODE_RWX, false, true);

    // 切换根页表
    mem_switch_root(root);
    log_info("根页表切换完成!");

    void *post_init_vaddr = (void *)PA2KA(post_init);
    typedef void (*TestFuncType)(void);
    TestFuncType post_init_func = (TestFuncType)post_init_vaddr;
    log_info("跳转到内核虚拟地址空间中的post_init函数: %p", post_init_vaddr);
    post_init_func();
}

void pre_init(void) {
    kputs("\n");
    // 由于此时还在pre-init阶段
    // 内核页表还未完成, 但是虚拟地址位于高地址处
    // 所以此处设置时需先将其转换为物理地址
    init_logger(kputs, "SUSTCore-PreInit");

    log_info("初始化内存分配器...");
    init_allocator();

    log_info("内核所占内存: [%p, %p]", &skernel, &ekernel);
    log_info("内核代码段所占内存: [%p, %p]", &s_text, &e_text);
    log_info("内核只读数据段所占内存: [%p, %p]", &s_rodata, &e_rodata);
    log_info("内核数据段所占内存: [%p, %p]", &s_data, &e_data);
    log_info("内核初始化数据段所占内存: [%p, %p]", &s_sdata, &e_sdata);
    log_info("内核BSS段所占内存: [%p, %p]", &s_bss, &e_bss);
    log_info("内核SBSS段所占内存: [%p, %p]", &s_sbss, &e_sbss);

    log_info("预初始化架构相关...");
    arch_pre_init();
}

void test(void);
extern dword IVT[];

bool post_init_flag = false;

void post_init(void) {
    // 设置post_init标志
    post_init_flag = true;

    // 移动sp到高位内存
    // 首先读取sp
    umb_t sp;
    __asm__ volatile("mv %0, sp" : "=r"(sp));
    // 计算新的sp位置
    umb_t new_sp = sp + (umb_t)KERNEL_VA_OFFSET;
    // 设置sp
    __asm__ volatile("mv sp, %0" ::"r"(new_sp));

    // 首先, 重新设置logger的函数指针
    init_logger(kputs, "SUSTCore");

    // 让PMM重新设置其数据结构
    // 使得新的分配数据位于高地址处
    pmm_post_init();

    // 进入分配器的第二阶段
    init_allocator_stage2();

    // 然后进行后初始化工作
    log_info("后初始化架构相关...");
    arch_post_init();

    // 进行IVT只读化
    PagingTab root = mem_root();
    mem_modify_page_range_to_rx(root, &s_ivt, &e_ivt);
    flush_tlb();
    log_info("IVT段已只读化!");

    // 将低位内存用户态化
    mem_modify_page_range_u(root, (void *)0x0,
                            (void *)(phymem_sz & ~(PAGE_SIZE - 1)), true);
    flush_tlb();
    log_info("低位内存[%p, %p)已用户态化!", (void *)0x0,
             (void *)(phymem_sz & ~(PAGE_SIZE - 1)));

    // 最后执行main与terminate
    main();

    terminate();
}

/**
 * @brief 初始化
 *
 */
void init(void) {
    pre_init();

    MemRegion *const layout = arch_get_memory_layout();

    // 遍历layout并打印
    MemRegion *iter = layout;
    while (iter != nullptr) {
        log_info("内存区域: 地址=[%p, %p), 大小=0x%lx, 状态=%d", iter->addr,
                 (void *)((char *)iter->addr + iter->size), iter->size,
                 iter->status);
        iter = iter->next;
    }

    log_info("初始化物理内存管理器...");
    pmm_init(layout);

    // kernel_paging_setup会把我们带到post_init阶段
    kernel_paging_setup(layout);
}

/**
 * @brief 内核启动函数
 *
 */
void kernel_setup(void) {
    init();
}