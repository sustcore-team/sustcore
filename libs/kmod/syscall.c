/**
 * @file syscall.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用实现
 * @version alpha-1.0.0
 * @date 2025-11-30
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <kmod/syscall.h>
#include <sus/syscall.h>

umb_t syscall(int sysno, umb_t arg0, umb_t arg1, umb_t arg2, umb_t arg3, umb_t arg4, umb_t arg5, umb_t arg6)
{
	// 通过register关键字将参数传递到指定寄存器
	register umb_t a0 asm ("a0") = (umb_t)(arg0);
	register umb_t a1 asm ("a1") = (umb_t)(arg1);
	register umb_t a2 asm ("a2") = (umb_t)(arg2);
	register umb_t a3 asm ("a3") = (umb_t)(arg3);
	register umb_t a4 asm ("a4") = (umb_t)(arg4);
	register umb_t a5 asm ("a5") = (umb_t)(arg5);
	register umb_t a7 asm ("a6") = (umb_t)(arg6);
	register umb_t a6 asm ("a7") = (umb_t)(sysno);

	// 内联汇编调用ecall指令
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");

    // 返回值在a0
	return a0;
}

void exit(int code) {
    syscall(SYS_EXIT, (umb_t)(code), 0, 0, 0, 0, 0, 0);
}

void yield(void) {
    syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0, 0);
}

int puts(const char *str) {
    return (int)syscall(SYS_WRITE_SERIAL, (umb_t)(str), 0, 0, 0, 0, 0, 0);
}

void *mapmem(Capability cap) {
    // TODO: 实现映射内存系统调用
    return nullptr;
}