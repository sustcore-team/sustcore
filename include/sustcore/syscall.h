/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用接口
 * @version alpha-1.0.0
 * @date 2025-12-04
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#define SYSCALL_BASE (0xFFFF'0000)

#define SYS_EXIT          (SYSCALL_BASE + 0x01)
#define SYS_YIELD         (SYSCALL_BASE + 0x02)
#define SYS_LOG           (SYSCALL_BASE + 0x03)
#define SYS_FORK          (SYSCALL_BASE + 0x04)
#define SYS_GETPID        (SYSCALL_BASE + 0x05)
#define SYS_CREATE_THREAD (SYSCALL_BASE + 0x06)
#define SYS_YIELD_THREAD  (SYSCALL_BASE + 0x07)

#define SYS_CREATE_NOTIFICATION   (SYSCALL_BASE + 0x0D)
#define SYS_SIGNAL_NOTIFICATION   (SYSCALL_BASE + 0x0A)
#define SYS_UNSIGNAL_NOTIFICATION (SYSCALL_BASE + 0x0B)
#define SYS_CHECK_NOTIFICATION    (SYSCALL_BASE + 0x0C)
#define SYS_WAIT_NOTIFICATION     (SYSCALL_BASE + 0x08)

#define SYS_CAP_CLONE     (SYSCALL_BASE + 0x0E)
#define SYS_CAP_DOWNGRADE (SYSCALL_BASE + 0x0F)
#define SYS_CAP_DERIVE    (SYSCALL_BASE + 0x10)
#define SYS_LOOKUP_CAP    (SYSCALL_BASE + 0x11)
#define SYS_CAP_REMOVE    (SYSCALL_BASE + 0x12)

// 以SYS_UNSTABLE_BASE开头的系统调用为不稳定接口, 可能会在后续版本中更改或移除
#define SYS_UNSTABLE_BASE  (0xFFC0'0000)
#define SYS_WRITE_SERIAL   (SYS_UNSTABLE_BASE + 0x01)
#define SYS_CREATE_PROCESS (SYS_UNSTABLE_BASE + 0x02)
#define SYS_GROW_VMA       (SYS_UNSTABLE_BASE + 0x03)
