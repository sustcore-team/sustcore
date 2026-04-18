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

#include <arch/riscv64/csr.h>
#include <arch/riscv64/device/fdt_helper.h>
#include <arch/riscv64/device/misc.h>
#include <arch/riscv64/trait.h>
#include <kio.h>
#include <libfdt.h>
#include <sbi/sbi.h>
#include <sus/logger.h>
#include <sus/units.h>

int hart_id;
void *dtb_ptr;

void Riscv64Serial::serial_write_char(char ch) {
    sbi_dbcn_console_write_byte(ch);
}

void Riscv64Serial::serial_write_string(size_t len, const char *str) {
    sbi_dbcn_console_write(len, str);
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

    units::frequency hz = get_clock_freq();
    if (hz.to_hz() == 0) {
        // 时钟频率获取失败
        while (true);
    }

    loggers::DEVICE::DEBUG("时钟频率为 %d Hz", hz.to_hz());
}

void Riscv64Initialization::post_init(void) {
    // 我们希望50ms触发1次时钟中断(调试用)
    units::frequency freq = get_clock_freq();
    if (freq < 0) {
        // 使用QEMU virt机器的默认值10MHz
        freq = 10_MHz;
        loggers::DEVICE::ERROR("获取时钟频率失败, 使用默认值 %d Hz", freq);
    }
    loggers::DEVICE::INFO("时钟频率: %d Hz = %d KHz = %d MHz", freq.to_hz(), freq.to_khz(), freq.to_mhz());
    init_timer(freq, 100_Hz); 
    loggers::DEVICE::INFO("启用时钟中断...");

    // 开启中断
    Riscv64Interrupt::sti();
}