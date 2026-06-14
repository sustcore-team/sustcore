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
#include <arch/riscv64/device/misc.h>
#include <arch/riscv64/trait.h>
#include <device/int.h>
#include <logger.h>
#include <sbi/sbi.h>
#include <sus/logger.h>

#include <cstddef>

size_t hart_id;
void *dtb_ptr;

void Riscv64Serial::serial_write_char(char ch) {
    sbi_dbcn_console_write_byte(ch);
}

void Riscv64Serial::serial_write_string(size_t len, const char *str) {
    sbi_dbcn_console_write(len, str);
}

extern void env_setup();

extern "C" void c_setup(void) {
    loggers::SUSTCORE::INFO("进入内核 C 入口点!");
    env_setup();
    while (true);
}

void Riscv64Initialization::pre_init(void) {}

void Riscv64Initialization::promote_dtb_to_kpa(void) {}

void Riscv64Idle::idle()
{
    while(true);
}

void Riscv64Initialization::post_init(void) {
    units::frequency freq = get_clock_freq();
    if (freq.to_mhz() == 0) {
        // 使用QEMU virt机器的默认值10MHz
        freq = 10_MHz;
        loggers::DEVICE::ERROR("获取时钟频率失败, 使用默认值 %d Hz", freq);
    }
    loggers::DEVICE::INFO("时钟频率: %d Hz = %d KHz = %d MHz", freq.to_hz(), freq.to_khz(), freq.to_mhz());
}
