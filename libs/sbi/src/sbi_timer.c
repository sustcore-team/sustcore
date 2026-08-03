/**
 * @file sbi_timer.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现 RISC-V SBI Time 扩展的定时器编程调用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sbi/sbi.h>

SBIRet sbi_set_timer(qword stime_value) {
    return sbi_ecall(SBI_EID_TIME, SBI_SET_TIMER, (xlen_t)(stime_value), 0, 0, 0, 0, 0);
}