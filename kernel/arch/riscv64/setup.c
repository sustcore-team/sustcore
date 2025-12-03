/**
 * @file setup.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISCV64启动程序
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/device/device.h>
#include <arch/riscv64/device/misc.h>
#include <arch/riscv64/int/exception.h>
#include <arch/riscv64/int/isr.h>
#include <basec/logger.h>
#include <libfdt.h>
#include <mem/kmem.h>
#include <sbi/sbi.h>
#include <sus/arch.h>
#include <sus/bits.h>
#include <sus/boot.h>

int kputchar(int ch) {
    sbi_dbcn_console_write_byte((char)ch);
    return ch;
}

int pre_init_kputs(const char *str) {
    int len = strlen(str);
    sbi_dbcn_console_write((umb_t)len, (const void *)str);
    return len;
}

int post_init_kputs(const char *str) {
    if ((void *)str < (void *)KPHY_VA_OFFSET) {
        int len = strlen(str);
        sbi_dbcn_console_write((umb_t)len, (const void *)str);
        return len;
    } else if ((void *)str >= (void *)KPHY_VA_OFFSET &&
               (void *)str < (void *)KERNEL_VA_OFFSET)
    {
        int len = strlen(str);
        sbi_dbcn_console_write((umb_t)len, (const void *)KPA2PA(str));
        return len;
    }
    int len = strlen(str);
    sbi_dbcn_console_write((umb_t)len, (const void *)KA2PA(str));
    return len;
}

int kputs(const char *str) {
    if (post_init_flag) {
        return post_init_kputs(str);
    } else {
        return pre_init_kputs(str);
    }
}

int kprintf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    // 使用vbprintf实现
    int len = vbprintf(kputs, format, args);

    va_end(args);
    return len;
}

//------------------ 调试异常处理程序 --------

// 触发非法指令异常
__attribute__((noinline)) int trigger_illegal_instruction(void) {
    asm volatile(".word 0x00000000");  // 全零是非法指令
    asm volatile(".word 0x000000FF");  // 自定义非法指令
    int a = 2, b = 20;
    asm volatile(
        "mv t0, %1\n"
        "mv t1, %2\n"
        ".word 0x00FF00FF\n"
        "mv %0, t0\n"
        : "=r"(a)
        : "r"(a), "r"(b)
        : "t0", "t1");  // 自定义非法指令2
    log_info("计算结果: %d", a);
    return -1;
}

//------------------ 调试异常处理程序 --------

FDTDesc *fdt;
umb_t hart_id, dtb_ptr;

void arch_pre_init(void) {
    log_info("Hart ID: %u", (unsigned int)hart_id);
    log_info("DTB Ptr: 0x%016lx", (unsigned long)dtb_ptr);

    log_info("开始验证并初始化设备树...");
    fdt = device_check_initial(dtb_ptr);
    if (fdt == nullptr) {
        log_error("设备树校验失败");
    }
    log_info("设备树校验成功!");
}

void arch_post_init(void) {
    log_info("初始化中断向量表...");
    init_ivt();

    // log_info("打印设备树信息");
    // print_device_tree_detailed(fdt);

    // log_info("开始测试非法指令异常处理...");
    // int a = trigger_illegal_instruction();
    // log_info("非法指令异常测试结果: %d", a);

    log_info("启用中断...");
    sti();

    // 我们希望500ms触发1次时钟中断(调试用)
    // 下面第一个单位为Hz, 第二个单位为mHz(10^-3 Hz)
    int freq = get_clock_freq_hz();
    if (freq < 0) {
        // 使用QEMU virt机器的默认值10MHz
        freq = 10000000;
        log_error("获取时钟频率失败, 使用默认值 %d Hz", freq);
    }
    log_info("时钟频率: %d Hz = %d KHz = %d MHz", freq, freq / 1000,
             freq / 1000000);
    init_timer(freq, 2000);
    log_info("启用时钟中断...");
}

void arch_terminate(void) {
    SBIRet ret;
    ret = sbi_legacy_shutdown();
    if (ret.error) {
        log_error("关机失败!");
        return;
    }

    log_error("此处不应当被执行到!");
    while (true);
}

void c_setup(void) {
    kernel_setup();
}