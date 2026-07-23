/**
 * @file sbi_send_ipi.c
 * @author hyj0824 (2417179808@qq.com)
 * @brief 
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sbi/sbi.h>

SBIRet sbi_send_ipi(xlen_t hart_mask, xlen_t hart_mask_base) {
    return sbi_ecall(SBI_EID_SPI, SBI_SEND_IPI,
                     hart_mask,
                     hart_mask_base,
                     0, 0, 0, 0);
}