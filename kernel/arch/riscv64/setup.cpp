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
#include <arch/riscv64/trait.h>
#include <device/int.h>
#include <logger.h>
#include <sbi/sbi.h>
#include <sus/logger.h>
#include <sustcore/boot.h>

#include <cstddef>

size_t hart_id;
void *dtb_ptr;
BootInfoHeader *bootinfo_ptr;

void Riscv64SerialEarlySerial::serial_write_char(char ch) {
    sbi_dbcn_console_write_byte(ch);
}

void Riscv64SerialEarlySerial::serial_write_string(size_t len, const char *str) {
    sbi_dbcn_console_write(len, str);
}

extern void env_setup();

extern "C" void c_setup(void) {
    loggers::SUSTCORE::INFO("进入内核 C 入口点!");
    env_setup();
    while (true);
}

void Riscv64Initialization::pre_init(void) {}

void Riscv64Idle::idle()
{
    while(true);
}

void Riscv64Initialization::post_init(void) 
{
}
