/**
 * @file sbi_send_ipi.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 实现 RISC-V SBI IPI 扩展的处理器间中断发送。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <sbi/sbi.h>

SBIRet sbi_send_ipi(xlen_t hart_mask, xlen_t hart_mask_base) {
    return sbi_ecall(SBI_EID_SPI, SBI_SEND_IPI, hart_mask, hart_mask_base, 0, 0, 0, 0);
}