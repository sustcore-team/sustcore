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

#include <libfdt.h>
#include <sbi/sbi.h>

int hart_id;
void *dtb_ptr;

extern "C"
void c_setup(void) {
    sbi_dbcn_console_write_byte('R');
    while (true);
}