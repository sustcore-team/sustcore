/**
 * @file sbi.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief SBI接口实现
 * @version alpha-1.0.0
 * @date 2025-11-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <sbi/sbi.h>

SBIRet sbi_ecall(dword eid, dword fid, 
                 umb_t arg0, umb_t arg1,
                 umb_t arg2, umb_t arg3, 
                 umb_t arg4, umb_t arg5) {
	SBIRet ret;

	// 通过register关键字将参数传递到指定寄存器
	register umb_t a0 asm ("a0") = (umb_t)(arg0);
	register umb_t a1 asm ("a1") = (umb_t)(arg1);
	register umb_t a2 asm ("a2") = (umb_t)(arg2);
	register umb_t a3 asm ("a3") = (umb_t)(arg3);
	register umb_t a4 asm ("a4") = (umb_t)(arg4);
	register umb_t a5 asm ("a5") = (umb_t)(arg5);
	register umb_t a6 asm ("a6") = (umb_t)(fid);
	register umb_t a7 asm ("a7") = (umb_t)(eid);

	// 内联汇编调用ecall指令
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	
	// 将返回值存入结构体
    ret.error = a0;
	ret.value = a1;

	return ret;
}