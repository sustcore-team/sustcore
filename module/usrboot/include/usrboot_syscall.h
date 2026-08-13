/**
 * @file usrboot_syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief usrboot 使用的最小系统调用 ABI 与 ec_write 包装。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <stddef.h>

#define USRBOOT_SYSCALL_EC_WRITE 1
#define USRBOOT_SYSCALL_YIELD    2

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 将指定长度的用户字符流写入内核早期控制台。 */
static inline size_t ec_write(const char *data, size_t length) {
    register const char *arg0 __asm__("a0") = data;
    register size_t arg1 __asm__("a1")       = length;
    register size_t number __asm__("a7")      = USRBOOT_SYSCALL_EC_WRITE;
#if defined(__ARCH_RISCV64__)
    __asm__ volatile("ecall" : "+r"(arg0) : "r"(arg1), "r"(number) : "memory");
#elif defined(__ARCH_LOONGARCH64__)
    __asm__ volatile("syscall 0" : "+r"(arg0) : "r"(arg1), "r"(number) : "memory");
#else
#error unsupported usrboot architecture
#endif
    return (size_t)arg0;
}

/** @brief 主动放弃当前时间片，让出处理器给下一个就绪线程。 */
static inline void usrboot_yield(void) {
    register size_t arg0 __asm__("a0")   = 0;
    register size_t number __asm__("a7") = USRBOOT_SYSCALL_YIELD;
#if defined(__ARCH_RISCV64__)
    __asm__ volatile("ecall" : "+r"(arg0) : "r"(number) : "memory");
#elif defined(__ARCH_LOONGARCH64__)
    __asm__ volatile("syscall 0" : "+r"(arg0) : "r"(number) : "memory");
#else
#error unsupported usrboot architecture
#endif
}

#ifdef __cplusplus
}
#endif
