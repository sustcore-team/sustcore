/**
 * @file sbi_timer.c
 * @author hyj0824 (2417179808@qq.com)
 * @brief 
 * @version 1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sbi/sbi.h>

SBIRet sbi_set_timer(qword stime_value) {
    return sbi_ecall(SBI_EID_TIME, SBI_SET_TIMER,
                     (xlen_t)(stime_value),
                     0, 0, 0, 0, 0);
}