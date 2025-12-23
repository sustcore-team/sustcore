/**
 * @file syscall.c
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-12-02
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <basec/logger.h>
#include <cap/capability.h>
#include <cap/not_cap.h>
#include <cap/pcb_cap.h>
#include <cap/tcb_cap.h>
#include <mem/alloc.h>
#include <sus/boot.h>
#include <sus/syscall.h>
#include <syscall/syscall.h>
#include <syscall/uaccess.h>
#include <task/proc.h>

void sys_exit(CapPtr ptr, umb_t exit_code) {
    pcb_cap_exit(cur_proc, ptr);
    log_info("进程%d调用 exit 系统调用, 退出码: %lu", cur_proc->pid, exit_code);
}

void sys_yield(CapPtr ptr) {
    pcb_cap_yield(cur_proc, ptr);
}

pid_t sys_getpid(CapPtr ptr) {
    return pcb_cap_getpid(cur_proc, ptr);
}

// TODO: fork函数的实现需要考虑复制过程中能力的传递
// 目前实现的版本特别简单, 但是有着诸多问题
CapPtr sys_fork(CapPtr ptr) {
    CapPtr child_cap;
    PCB *child = pcb_cap_fork(cur_proc, ptr, &child_cap);
    if (child == nullptr) {
        return INVALID_CAP_PTR;
    }
    // 我们需要为子进程设置返回值
    // 对子进程, 设置副返回值为0
    // 主返回值为子进程的Capability
    arch_setup_argument(child->main_thread, 0, child_cap.val);
    arch_setup_argument(child->main_thread, 1, 0);
    // TODO: 为子进程传递主线程的Capability

    // 主进程的副返回值为子进程的PID
    arch_setup_argument(cur_proc->current_thread, 1, (umb_t)(child->pid));
    // 移动子进程的pc
    // TODO: 这里假设每条指令长度为4字节
    // 应当通过架构相关方法获得
    *(child->main_thread->ip) += 4;
    // 返回子进程的能力
    // 父进程对子进程享有同等权限
    return pcb_cap_clone(child, child_cap, cur_proc);
}

void sys_log(const char *msg) {
    int len    = ua_strlen(msg);
    char *kmsg = (char *)kmalloc(len + 1);
    if (kmsg == nullptr) {
        log_info("sys_log: 分配内核缓冲区失败");
        return;
    }
    ua_strcpy(kmsg, msg);
    log_info("用户进程(pid = %d)日志: %s", cur_proc->pid, kmsg);
    kfree(kmsg);
}

// TODO: 这个功能本应交给对应驱动
// 但目前还没有实现串口驱动, 先放在这里
int sys_write_serial(const char *msg) {
    int len    = ua_strlen(msg);
    char *kmsg = (char *)kmalloc(len + 1);
    if (kmsg == nullptr) {
        log_info("sys_write_serial: 分配内核缓冲区失败");
        return 0;
    }
    ua_strcpy(kmsg, msg);
    int ret = kputs(kmsg);
    kfree(kmsg);
    return ret;
}

CapPtr sys_create_thread(CapPtr ptr, void *entrypoint, int priority) {
    // 创建新线程
    CapPtr tcb_ptr = pcb_cap_create_thread(cur_proc, ptr, entrypoint, priority);
    // 也将该能力传递给新线程
    TCB *tcb = tcb_cap_unwrap(cur_proc, tcb_ptr);
    arch_setup_argument(tcb, 0, tcb_ptr.val);
    return tcb_ptr;
}

bool sys_wait_notification(PCB *p, CapPtr pcb_ptr, CapPtr not_ptr,
                           qword *wait_bitmap) {
    // 从进程的空间中复制wait_bitmap
    qword buffer[NOTIFICATION_BITMAP_QWORDS];
    ua_memcpy(buffer, wait_bitmap, sizeof(qword) * NOTIFICATION_BITMAP_QWORDS);
    // 设置等待通知
    return pcb_cap_wait_notification(p, pcb_ptr, not_ptr, buffer);
}

bool sys_wait_notification_thread(PCB *p, CapPtr tcb_ptr, CapPtr not_ptr,
                                  qword *wait_bitmap) {
    // 从进程的空间中复制wait_bitmap
    qword buffer[NOTIFICATION_BITMAP_QWORDS];
    ua_memcpy(buffer, wait_bitmap, sizeof(qword) * NOTIFICATION_BITMAP_QWORDS);
    // 设置等待通知
    return tcb_cap_wait_notification(p, tcb_ptr, not_ptr, buffer);
}

void sys_set_notification(PCB *p, CapPtr ptr, int notification_id) {
    not_cap_set(p, ptr, notification_id);
}

void sys_reset_notification(PCB *p, CapPtr ptr, int notification_id) {
    not_cap_reset(p, ptr, notification_id);
}

bool sys_check_notification(PCB *p, CapPtr ptr, int notification_id) {
    return not_cap_check(p, ptr, notification_id);
}

void sys_yield_thread(CapPtr ptr) {
    tcb_cap_yield(cur_proc, ptr);
}

umb_t syscall_handler(int sysno, RegCtx *ctx, ArgumentGetter arg_getter) {
    // 第0个参数一定存放Capability
    CapPtr cap;
    cap.val = arg_getter(ctx, 0);
    switch (sysno) {
        case SYS_EXIT:  sys_exit(cap, arg_getter(ctx, 1)); return 0;
        case SYS_YIELD: sys_yield(cap); return 0;
        case SYS_LOG:   sys_log((const char *)arg_getter(ctx, 1)); return 0;
        case SYS_WRITE_SERIAL:
            int ret = sys_write_serial((const char *)arg_getter(ctx, 1));
            return ret;
        case SYS_FORK: {
            CapPtr child_cap = sys_fork(cap);
            return child_cap.val;
        }
        case SYS_GETPID: {
            pid_t pid = sys_getpid(cap);
            return (umb_t)pid;
        }
        case SYS_CREATE_THREAD: {
            CapPtr thread_cap = sys_create_thread(
                cap, (void *)arg_getter(ctx, 1), (int)(arg_getter(ctx, 2)));
            return thread_cap.val;
        }
        case SYS_YIELD_THREAD: {
            sys_yield_thread(cap);
            return 0;
        }
        case SYS_WAIT_NOTIFICATION: {
            bool res = sys_wait_notification(
                cur_proc, cap,
                (CapPtr){.val = arg_getter(ctx, 1)},
                (qword *)arg_getter(ctx, 2));
            return (umb_t)(res ? 1 : 0);
        }
        case SYS_WAIT_NOTIFICATION_THREAD: {
            bool res = sys_wait_notification_thread(
                cur_proc, cap,
                (CapPtr){.val = arg_getter(ctx, 1)},
                (qword *)arg_getter(ctx, 2));
            return (umb_t)(res ? 1 : 0);
        }
        case SYS_SET_NOTIFICATION: {
            sys_set_notification(
                cur_proc, cap, (int)(arg_getter(ctx, 1)));
            return 0;
        }
        case SYS_RESET_NOTIFICATION: {
            sys_reset_notification(
                cur_proc, cap, (int)(arg_getter(ctx, 1)));
            return 0;
        }
        case SYS_CHECK_NOTIFICATION: {
            bool res = sys_check_notification(
                cur_proc, cap, (int)(arg_getter(ctx, 1)));
            return (umb_t)(res ? 1 : 0);
        }
        default: log_info("未知系统调用号: %d", sysno); return (umb_t)(-1);
    }
}