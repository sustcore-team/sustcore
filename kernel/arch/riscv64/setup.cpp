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

#include <arch/riscv64/configuration.h>
#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/device/misc.h>
#include <arch/riscv64/trait.h>
#include <kio.h>
#include <libfdt.h>
#include <sbi/sbi.h>
#include <basecpp/logger.h>

int hart_id;
void *dtb_ptr;

void Riscv64Serial::serial_write_char(char ch) {
    sbi_dbcn_console_write_byte(ch);
}

void Riscv64Serial::serial_write_string(const char *str) {
    sbi_dbcn_console_write(strlen(str), str);
}

extern "C" void c_setup(void) {
    kernel_setup();
    while (true);
}

void Riscv64Initialization::pre_init(void) {
    if (FDTHelper::fdt_init(dtb_ptr) == nullptr) {
        // FDT初始化失败
        while (true);
    }

    int hz = get_clock_freq_hz();
    if (hz <= 0) {
        // 时钟频率获取失败
        while (true);
    }

    DEVICE.DEBUG("时钟频率为 %d Hz", hz);
}

// 触发非法指令异常
__attribute__((noinline)) int trigger_illegal_instruction(void) {
    asm volatile(".word 0x00000000");  // 全零是非法指令
    asm volatile(".word 0x000000FF");  // 自定义非法指令
    int a = 3, b = 3;
    asm volatile(
        "mv t0, %1\n"
        "mv t1, %2\n"
        ".word 0x00FF00FF\n"
        "mv %0, t0\n"
        : "=r"(a)
        : "r"(a), "r"(b)
        : "t0", "t1");  // 自定义非法指令2
    INTERRUPT.INFO("计算结果: %d", a);
    return -1;
}

void Riscv64Initialization::post_init(void) {
    // 测试非法指令异常处理
    trigger_illegal_instruction();

    // 我们希望50ms触发1次时钟中断(调试用)
    // 下面第一个单位为Hz, 第二个单位为mHz(10^-3 Hz)
    int freq = get_clock_freq_hz();
    if (freq < 0) {
        // 使用QEMU virt机器的默认值10MHz
        freq = 10000000;
        DEVICE.ERROR("获取时钟频率失败, 使用默认值 %d Hz", freq);
    }
    DEVICE.INFO("时钟频率: %d Hz = %d KHz = %d MHz", freq, freq / 1000,
             freq / 1000000);
    init_timer(freq, 20000);
    DEVICE.INFO("启用时钟中断...");

    // 开启中断
    ArchInterrupt::sti();
}