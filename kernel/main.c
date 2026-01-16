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

#include <arch/riscv64/paging.h>
#include <basec/logger.h>
#include <elfloader.h>
#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/buddy.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sus/arch.h>
#include <sus/boot.h>
#include <sus/list_helper.h>
#include <sus/symbols.h>
#include <task/proc.h>
#include <task/scheduler.h>

#define KERNEL_IOCHAN (8)

int basec_puts(iochan_t chan, const char *str)
{
    if (chan == KERNEL_IOCHAN) {
        // 内核通道, 输出到串口
        kputs(str);
        return 0;
    }
    kprintf("Unknown iochan: %d\n", chan);
    return -1;
}

void assertion_failure(const char *expression, const char *file,
                       const char *base_file, int line) {
    log_error("In %s(from %s), line %d, assert failed: %s ", file, base_file,
              line, expression);
}

void panic_failure(const char *expression, const char *file,
                   const char *base_file, int line) {
    log_error("In %s(from %s), line %d, assert failed: %s ", file, base_file,
              line, expression);
    while (true);
}

void panic(const char *format, ...) {
    log_error("PANIC!");
    while (true);
}

/**
 * @brief 内核主函数
 *
 * @return int
 */
int main(void) {
    // 读取attach.license段
    // size_t license_size =
    //     (size_t)((umb_t)&e_attach_license - (umb_t)&s_attach_license);
    // license_size      = license_size > 2048 ? 2048 : license_size;
    // char *license_str = (char *)kmalloc(license_size + 1);
    // memcpy(license_str, &s_attach_license, license_size);
    // license_str[license_size] = '\0';
    // log_info("内核附加文件: license");
    // kprintf("----- BEGIN LICENSE -----\n");
    // kprintf("%s\n", license_str);
    // kprintf("-----  END LICENSE  -----\n");
    // kfree(license_str);

    log_info("Hello RISCV World!!");

    program_info test_prog = load_elf_program(&s_attach_test);
    log_info("ELF程序信息:");
    log_info("  入口点: %p", test_prog.entrypoint);
    log_info("  程序整体: [%p, %p)", test_prog.program_start,
             test_prog.program_end);

    // 创建测试进程
    PCB *p = new_task(test_prog.tm,             // 进程内存信息
                      test_prog.program_start,  // 栈起始地址
                      test_prog.program_end,    // 堆起始地址
                      test_prog.entrypoint, 2, nullptr);

    // 输出各项信息
    log_info("创建测试进程完成: PID=%d", p->pid);
    log_info("  入口点: %p", p->main_thread->entrypoint);
    // 遍历VMA
    VMA *vma;
    foreach_ordered_list(vma, TM_VMA_LIST(p->tm)) {
        const char *type_str[] = {
            "VMAT_NONE",     "VMAT_CODE",     "VMAT_DATA",     "VMAT_STACK",
            "VMAT_HEAP",     "VMAT_MMAP",     "VMAT_SHARE_RW", "VMAT_SHARE_RO",
            "VMAT_SHARE_RX", "VMAT_SHARE_RWX"};
        log_info("  VMA: vaddr=%p size=%lu type=%s", vma->vaddr, vma->size,
                 type_str[vma->type]);
    }
    log_info("  内核栈: %p", p->main_thread->kstack);
    log_info("  上下文: %p", p->main_thread->ctx);
    log_info("  初始ip: %p", *p->main_thread->ip);
    log_info("  初始sp: %p", *p->main_thread->sp);
    log_info("  rp级别: %d", p->rp_level);
    log_info("  页表: %p(paddr:%p)", p->tm->pgd, KPA2PA(p->tm->pgd));
    log_info("页表布局如下:");
    mem_display_mapping_layout(p->tm->pgd);

    log_info("启动进程调度器...");

    // 启动调度器
    sheduling_enabled = true;

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
    // 初始化内核分页信息
    setup_kernel_paging(layout);
    kfree(layout);

    // 构造根页表
    void *root = mem_construct_root();
    log_info("构造根页表完成: %p", root);

    // 创建内核分页
    create_kernel_paging(root);

    // 对[0, upper_bound)作恒等映射(U-Mode)
    mem_maps_range_to(root, (void *)0x0, (void *)0x0, phymem_sz / PAGE_SIZE,
                      RWX_MODE_RWX, false, true);

    // 切换根页表
    mem_switch_root(root);
    log_info("根页表切换完成!");

    // 计算post_init函数在内核虚拟地址空间中的地址
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
    init_logger(KERNEL_IOCHAN, "SUSTCore-PreInit");

    log_info("初始化内存分配器...");
    init_allocator();

    log_info("内核所占内存: [%p, %p]", &skernel, &ekernel);
    log_info("内核代码段所占内存: [%p, %p]", &s_text, &e_text);
    log_info("内核只读数据段所占内存: [%p, %p]", &s_rodata, &e_rodata);
    log_info("内核数据段所占内存: [%p, %p]", &s_data, &e_data);
    log_info("内核BSS段所占内存: [%p, %p]", &s_bss, &e_bss);

    log_info("预初始化架构相关...");
    arch_pre_init();
}

void test(void);

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
    init_logger(KERNEL_IOCHAN, "SUSTCore");

    // 让Buddy重新设置其数据结构
    // 使得新的分配数据位于高地址处
    buddy_post_init();

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

    // 初始化进程管理系统
    log_info("初始化进程管理系统...");
    proc_init();
    log_info("进程管理系统初始化完成!");

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
    buddy_init(layout);

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