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

#define SYSCALL_BASE (0xFFFF0000)

#define SYS_PCB_KILL      (SYSCALL_BASE + 0x01)
#define SYS_YIELD         (SYSCALL_BASE + 0x02)
#define SYS_LOG           (SYSCALL_BASE + 0x03)
#define SYS_FORK          (SYSCALL_BASE + 0x04)
#define SYS_GETPID        (SYSCALL_BASE + 0x05)
#define SYS_CREATE_THREAD (SYSCALL_BASE + 0x06)
#define SYS_YIELD_THREAD  (SYSCALL_BASE + 0x07)
#define SYS_EXECVE        (SYSCALL_BASE + 0x08)
#define SYS_PCB_MAP       (SYSCALL_BASE + 0x09)

#define SYS_NOTIF_CREATE   (SYSCALL_BASE + 0x0A)
#define SYS_NOTIF_SIGNAL   (SYSCALL_BASE + 0x0B)
#define SYS_NOTIF_UNSIGNAL (SYSCALL_BASE + 0x0C)
#define SYS_NOTIF_CHECK    (SYSCALL_BASE + 0x0D)
#define SYS_NOTIF_WAIT     (SYSCALL_BASE + 0x0E)

#define SYS_CAP_CLONE     (SYSCALL_BASE + 0x0F)
#define SYS_CAP_DOWNGRADE (SYSCALL_BASE + 0x10)
#define SYS_CAP_DERIVE    (SYSCALL_BASE + 0x11)
#define SYS_CAP_LOOKUP    (SYSCALL_BASE + 0x12)
#define SYS_CAP_REMOVE    (SYSCALL_BASE + 0x13)

#define SYS_ENDPOINT_CREATE     (SYSCALL_BASE + 0x14)
#define SYS_ENDPOINT_SEND       (SYSCALL_BASE + 0x15)
#define SYS_ENDPOINT_RECV       (SYSCALL_BASE + 0x16)
#define SYS_ENDPOINT_SEND_ASYNC (SYSCALL_BASE + 0x17)
#define SYS_ENDPOINT_RECV_ASYNC (SYSCALL_BASE + 0x18)
#define SYS_ENDPOINT_CALL       (SYSCALL_BASE + 0x19)
#define SYS_ENDPOINT_REPLY      (SYSCALL_BASE + 0x1A)

#define SYS_MEM_CREATE (SYSCALL_BASE + 0x1B)
#define SYS_MEM_UNMAP  (SYSCALL_BASE + 0x1C)
#define SYS_MEM_RESIZE (SYSCALL_BASE + 0x1D)
#define SYS_MEM_QUERY  (SYSCALL_BASE + 0x1E)

// 以SYS_UNSTABLE_BASE开头的系统调用为不稳定接口, 可能会在后续版本中更改或移除
#define SYS_UNSTABLE_BASE  (0xFFC00000)
#define SYS_WRITE_SERIAL   (SYS_UNSTABLE_BASE + 0x01)
#define SYS_CREATE_PROCESS (SYS_UNSTABLE_BASE + 0x02)
