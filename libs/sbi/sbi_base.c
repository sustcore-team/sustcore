/**
 * @file sbi_base.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI Base扩展实现
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sbi/sbi.h>

SBIRet sbi_get_spec_version(void) {
	return sbi_ecall(SBI_EID_BASE, SBI_GET_SPEC_VERSION, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_get_impl_id(void) {
	return sbi_ecall(SBI_EID_BASE, SBI_GET_IMPL_ID, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_get_impl_version(void) {
	return sbi_ecall(SBI_EID_BASE, SBI_GET_IMPL_VERSION, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_probe_extension(dword extension_id) {
	return sbi_ecall(SBI_EID_BASE, SBI_PROBE_EXTENSION, 
					(umb_t)(extension_id),
					0, 0, 0, 0, 0);
}

SBIRet sbi_get_mvendorid(void) {
	return sbi_ecall(SBI_EID_BASE, SBI_GET_MVENDORID, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_get_marchid(void) {
	return sbi_ecall(SBI_EID_BASE, SBI_GET_MARCHID, 
					0, 0, 0, 0, 0, 0);
}

SBIRet sbi_get_mimpid(void) {
	return sbi_ecall(SBI_EID_BASE, SBI_GET_MIMPID, 
					0, 0, 0, 0, 0, 0);
}