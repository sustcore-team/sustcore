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
#include <arch/trait.h>
#include <libfdt.h>
#include <sbi/sbi.h>

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