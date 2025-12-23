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

#define SYS_EXIT                     (SYSCALL_BASE + 0x01)
#define SYS_YIELD                    (SYSCALL_BASE + 0x02)
#define SYS_LOG                      (SYSCALL_BASE + 0x03)
#define SYS_FORK                     (SYSCALL_BASE + 0x04)
#define SYS_GETPID                   (SYSCALL_BASE + 0x05)
#define SYS_CREATE_THREAD            (SYSCALL_BASE + 0x06)
#define SYS_YIELD_THREAD             (SYSCALL_BASE + 0x07)
#define SYS_WAIT_NOTIFICATION        (SYSCALL_BASE + 0x08)
#define SYS_WAIT_NOTIFICATION_THREAD (SYSCALL_BASE + 0x09)
#define SYS_SET_NOTIFICATION         (SYSCALL_BASE + 0x0A)
#define SYS_RESET_NOTIFICATION       (SYSCALL_BASE + 0x0B)
#define SYS_CHECK_NOTIFICATION       (SYSCALL_BASE + 0x0C)

// 以SYS_UNSTABLE_BASE开头的系统调用为不稳定接口, 可能会在后续版本中更改或移除
#define SYS_UNSTABLE_BASE (0xFFC0'0000)
#define SYS_WRITE_SERIAL  (SYS_UNSTABLE_BASE + 0x01)