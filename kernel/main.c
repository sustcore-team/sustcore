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

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include <basec/logger.h>
#include <sbi/sbi.h>
#include <libfdt.h>

#include <arch/riscv64/int/isr.h>
#include <arch/riscv64/int/exception.h>

#include <mem/alloc.h>
#include <mem/pmm.h>

int kputchar(int ch) {
    sbi_dbcn_console_write_byte((char)ch);
    return ch;
}

int kputs(const char *str) {
    int len = strlen(str);
    sbi_dbcn_console_write((umb_t)len, (const void*)str);
    return len;
}

umb_t hart_id, dtb_ptr;
void *fdt;

//------------------ 调试libfdt使用-----------

/* 基础验证 */
int fdt_check_initial(void *fdt) {
    /* 检查魔数 */
    if (fdt_magic(fdt) != FDT_MAGIC) {
        return -FDT_ERR_BADMAGIC;
    }
    
    /* 检查版本兼容性 */
    if (fdt_version(fdt) < FDT_FIRST_SUPPORTED_VERSION) {
        return -FDT_ERR_BADVERSION;
    }
    
    /* 完整检查 */
    return fdt_check_header(fdt);
}

/** 遍历节点 */
void traverse_nodes(void *fdt) {
    int node, depth = 0;
    
    /* 从根节点开始遍历 */
    for (node = fdt_next_node(fdt, -1, &depth);
         node >= 0;
         node = fdt_next_node(fdt, node, &depth)) {
        
        const char *name = fdt_get_name(fdt, node, NULL);
        log_debug("Node: %s (depth: %d)", name, depth);
    }
}

//------------------ 调试libfdt使用-----------

//------------------ 调试异常处理程序 --------

// 触发非法指令异常
__attribute__((noinline))
int trigger_illegal_instruction(void) {
    asm volatile (".word 0x00000000");  // 全零是非法指令
    asm volatile (".word 0x000000FF");  // 自定义非法指令
    int a = 2, b = 20;
    asm volatile (
        "mv t0, %1\n"
        "mv t1, %2\n"
        ".word 0x00FF00FF\n"
        "mv %0, t0\n" 
        : "=r"(a) : "r"(a), "r"(b) : "t0", "t1");  // 自定义非法指令2
    log_info("计算结果: %d", a);
    return -1;
}

//------------------ 调试异常处理程序 --------

/**
 * @brief 内核主函数
 * 
 * @return int 
 */
int main(void) {
    log_info("Hello RISCV World!");
    log_info("Hart ID: %u", (unsigned int)hart_id);
    log_info("DTB Ptr: 0x%016lx", (unsigned long)dtb_ptr);

    if (fdt != nullptr) {
        log_info("开始遍历设备树节点...");
        traverse_nodes(fdt);
        log_info("设备树节遍历完成!");
    }
    else {
        log_error("设备树指针无效, 跳过!");
    }

    log_info("开始测试非法指令异常处理...");
    int a = trigger_illegal_instruction();
    log_info("非法指令异常测试结果: %d", a);

    log_info("启用中断...");
    sti();

    // TODO: 通过设备树获取时钟频率
    // 正常来说, 我们应该要查询设备树
    // 但是对QEMU, virt机器的频率始终为10MHz
    // 所以这里先硬编码为10MHz
    // 同时, 我们希望2s触发1次时钟中断(调试用)
    // 下面第一个单位为Hz, 第二个单位为mHz(10^-3 Hz)
    init_timer(10000000, 500); // 10MHz
    log_info("启用时钟中断...");
    while(true);

    return 0;
}

/**
 * @brief 初始化
 * 
 */
void init(void) {
    kputs("\n");

    init_logger(kputs, "SUSTCore");

    log_info("初始化中断向量表...");
    init_ivt();

    fdt = (void *)dtb_ptr;
    log_info("开始验证设备树...");
    int ret = fdt_check_initial(fdt);
    if (ret != 0) {
        log_error("设备树校验失败: %d", ret);
        fdt = nullptr;
    }
    log_info("设备树校验成功!");

    log_info("初始化内存分配器...");

    init_allocator();
    pmm_init(nullptr);
}

/**
 * @brief 收尾工作
 * 
 */
void terminate(void) {
    SBIRet ret;
    ret = sbi_legacy_shutdown();
    if (ret.error) {
        log_error("关机失败!");
    } 
    
    log_error("何意味?关机成功了?!你又是怎么溜达到这来的?!!");
    while(true);
}

/**
 * @brief 内核启动函数
 *
 */
void c_setup(void) {
    init();

    main();

    terminate();
}