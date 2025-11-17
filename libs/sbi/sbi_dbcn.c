/**
 * @file sbi_dbcn.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI DBCN扩展实现
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sbi/sbi.h>

SBIRet sbi_dbcn_console_write(umb_t len, const void* buf) {
	return sbi_ecall(SBI_EID_DBCN, SBI_CONSOLE_WRITE, 
					len, (umb_t)(buf),
					0, 0, 0, 0);
}

SBIRet sbi_dbcn_console_read(umb_t len, void* buf) {
	return sbi_ecall(SBI_EID_DBCN, SBI_CONSOLE_READ,
					len, (umb_t)(buf),
					0, 0, 0, 0);
}

SBIRet sbi_dbcn_console_write_byte(char ch) {
	return sbi_ecall(SBI_EID_DBCN, SBI_CONSOLE_WRITE_BYTE, 
					(umb_t)(ch),
					0, 0, 0, 0, 0);
}