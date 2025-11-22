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
#include <mem/pmm.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sus/arch.h>
#include <sus/boot.h>
#include <sus/symbols.h>
#include <mem/kmem.h>

/**
 * @brief 内核主函数
 *
 * @return int
 */
int main(void) {
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
    log_info("内存地址上界:   %p", upper_bound);
    // 对[0, upper_bound)作恒等映射
    mem_maps_range_to(root, (void *)0x0, (void *)0x0,
                         (umb_t)upper_bound / PAGE_SIZE, RWX_MODE_RWX, false,
                         true);

    // 把内核部分映射到高地址
    umb_t kernel_pages =
        ((umb_t)&ekernel - (umb_t)&skernel + PAGE_SIZE - 1) / PAGE_SIZE;

    log_info("内核地址偏移:      %p", (umb_t)KERNEL_VA_OFFSET);
    void *kernel_vaddr_start = (void *)(((umb_t)&skernel) + (umb_t)KERNEL_VA_OFFSET);
    log_info("内核虚拟地址空间: [%p, %p)", kernel_vaddr_start,
             (void *)((umb_t)kernel_vaddr_start + kernel_pages * PAGE_SIZE));

    mem_maps_range_to(root, kernel_vaddr_start, (&skernel),
                         kernel_pages, RWX_MODE_RWX, false, true);

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

    log_info("预初始化架构相关...");
    arch_pre_init();
}

void post_init(void) {
    // 首先, 重新设置logger的函数指针
    init_logger(kputs, "SUSTCore");

    // 进入分配器的第三阶段
    // 让PMM重新设置其数据结构
    // 使得新的分配数据位于高地址处

    // 然后进行后初始化工作
    log_info("后初始化架构相关...");
    arch_post_init();

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