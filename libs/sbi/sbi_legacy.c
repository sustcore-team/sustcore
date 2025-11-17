/**
 * @file sbi_legacy.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI Legacy接口实现
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sbi/sbi.h>

// Legacy SBI Calls
SBIRet sbi_set_timer(qword stime_value) {
	return sbi_ecall(SBI_EID_SET_TIMER, 0, 
					(umb_t)(stime_value), 
					0, 0, 0, 0, 0);
}

SBIRet sbi_console_putchar(char ch) {
    return sbi_ecall(SBI_EID_CONSOLE_PUTCHAR, 0, 
					(umb_t)(ch),
					0, 0, 0, 0, 0);
}

SBIRet sbi_console_getchar(void) {
	return sbi_ecall(SBI_EID_CONSOLE_GETCHAR, 0, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_clear_ipi(void) {
	return sbi_ecall(SBI_EID_CLEAR_IPI, 0, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_send_ipi(const void *hart_mask_ptr) {
	return sbi_ecall(SBI_EID_SEND_IPI, 0, 
					(umb_t)(hart_mask_ptr),
					0, 0, 0, 0, 0);
}

SBIRet sbi_remote_fence_i(const void *hart_mask_ptr) {
	return sbi_ecall(SBI_EID_REMOTE_FENCE_I, 0, 
					(umb_t)(hart_mask_ptr),
					0, 0, 0, 0, 0);
}

SBIRet sbi_remote_sfence_vma(const void *hart_mask_ptr, 
							 umb_t start_addr, 
							 umb_t size) {
	return sbi_ecall(SBI_EID_REMOTE_SFENCE_VMA, 0, 
					(umb_t)(hart_mask_ptr),
					start_addr,
					size,
					0, 0, 0);
}

SBIRet sbi_remote_sfence_vma_asid(const void *hart_mask_ptr, 
								 umb_t start_addr, 
								 umb_t size,
								 umb_t asid) {
	return sbi_ecall(SBI_EID_REMOTE_SFENCE_VMA_ASID, 0, 
					(umb_t)(hart_mask_ptr),
					start_addr,
					size,
					asid,
					0, 0);
}

SBIRet sbi_shutdown(void) {
    return sbi_ecall(SBI_EID_SHUTDOWN, 0, 
					0, 0, 0, 0, 0, 0);
}