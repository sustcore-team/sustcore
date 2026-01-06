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

const char *cap_ptr_to_string(PCB *p, CapPtr ptr, int i) {
    static char buffer_1[256];
    static char buffer_2[256];
    static char buffer_3[256];
    static char buffer_4[256];

    Capability *cap = fetch_cap(p, ptr);

    switch (i) {
        case 0:
            sprintf(buffer_1, "CapPtr[1st](cspace=%u, cindex=%u. type=%s)",
                    ptr.cspace, ptr.cindex, cap_type_to_string(cap->type));
            return buffer_1;
        case 1:
            sprintf(buffer_2, "CapPtr[2nd](cspace=%u, cindex=%u)", ptr.cspace,
                    ptr.cindex, cap_type_to_string(cap->type));
            return buffer_2;
        case 2:
            sprintf(buffer_3, "CapPtr[3rd](cspace=%u, cindex=%u)", ptr.cspace,
                    ptr.cindex, cap_type_to_string(cap->type));
            return buffer_3;
        case 3:
            sprintf(buffer_4, "CapPtr[4th](cspace=%u, cindex=%u)", ptr.cspace,
                    ptr.cindex, cap_type_to_string(cap->type));
            return buffer_4;
        default: return "Invalid index";
    }
}

void sys_exit(PCB *p, TCB *t, CapPtr ptr, umb_t exit_code) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_exit(%s, %d)", p->pid, t->tid,
              cap_ptr_to_string(p, ptr, 0), exit_code);

    pcb_cap_exit(p, ptr);
    log_info("线程程(pid=%d, tid=%d)调用 exit 系统调用, 退出码: %lu", p->pid,
             t->tid, exit_code);
}

void sys_yield(PCB *p, TCB *t, CapPtr ptr) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_yield(%s)", p->pid, t->tid,
              cap_ptr_to_string(p, ptr, 0));

    log_info("sys_yield()已被废弃, 请使用 sys_yield_thread()\n");
}

pid_t sys_getpid(PCB *p, TCB *t, CapPtr ptr) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_getpid(%s)", p->pid, t->tid,
              cap_ptr_to_string(p, ptr, 0));
    return pcb_cap_getpid(p, ptr);
}

// TODO: fork函数的实现需要考虑复制过程中能力的传递
// 目前实现的版本特别简单, 但是有着诸多问题
CapPtr sys_fork(PCB *p, TCB *t, CapPtr ptr) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_fork(%s)", p->pid, t->tid,
              cap_ptr_to_string(p, ptr, 0));

    CapPtr child_cap;
    PCB *child = pcb_cap_fork(p, ptr, &child_cap);
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
    arch_setup_argument(t, 1, (umb_t)(child->pid));
    // 移动子进程的pc
    // TODO: 这里假设每条指令长度为4字节
    // 应当通过架构相关方法获得
    *(child->main_thread->ip) += 4;
    // 返回子进程的能力
    // 父进程对子进程享有同等权限
    return pcb_cap_clone(child, child_cap, p);
}

void sys_log(PCB *p, TCB *t, const char *msg) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_log(%p)", p->pid, t->tid, msg);

    int len    = ua_strlen(msg);
    char *kmsg = (char *)kmalloc(len + 1);
    if (kmsg == nullptr) {
        log_info("sys_log: 分配内核缓冲区失败");
        return;
    }
    ua_strcpy(kmsg, msg);
    log_info("用户线程(pid=%d, tid = %d)日志: %s", p->pid, t->tid, kmsg);
    kfree(kmsg);
}

// TODO: 这个功能本应交给对应驱动
// 但目前还没有实现串口驱动, 先放在这里
int sys_write_serial(PCB *p, TCB *t, const char *msg) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_write_serial(%p)", p->pid, t->tid,
              msg);

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

CapPtr sys_create_thread(PCB *p, TCB *t, CapPtr ptr, void *entrypoint,
                         int priority) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_create_thread(%s, %p, %d)", p->pid,
              t->tid, cap_ptr_to_string(p, ptr, 0), entrypoint, priority);
    // 创建新线程
    CapPtr tcb_ptr = pcb_cap_create_thread(p, ptr, entrypoint, priority);
    // 也将该能力传递给新线程
    TCB *tcb       = tcb_cap_unwrap(p, tcb_ptr);
    arch_setup_argument(tcb, 0, tcb_ptr.val);
    return tcb_ptr;
}

bool sys_wait_notification(PCB *p, TCB *t, CapPtr pcb_ptr, CapPtr not_ptr,
                           qword *wait_bitmap) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_wait_notification(%s, %s, %p)",
              p->pid, t->tid, cap_ptr_to_string(p, pcb_ptr, 0),
              cap_ptr_to_string(p, not_ptr, 1), wait_bitmap);
    log_info(
        "sys_wait_notification()已被废弃, 请使用 "
        "sys_wait_notification_thread()\n");
    return false;
}

bool sys_wait_notification_thread(PCB *p, TCB *t, CapPtr tcb_ptr,
                                  CapPtr not_ptr, qword *wait_bitmap) {
    log_debug(
        "线程(pid=%d, tid=%d) 调用 sys_wait_notification_thread(%s, %s, %p)",
        p->pid, t->tid, cap_ptr_to_string(p, tcb_ptr, 0),
        cap_ptr_to_string(p, not_ptr, 1), wait_bitmap);
    // 从进程的空间中复制wait_bitmap
    qword buffer[NOTIFICATION_BITMAP_QWORDS];
    ua_memcpy(buffer, wait_bitmap, sizeof(qword) * NOTIFICATION_BITMAP_QWORDS);
    // 设置等待通知
    return tcb_cap_wait_notification(p, tcb_ptr, not_ptr, buffer);
}

void sys_set_notification(PCB *p, TCB *t, CapPtr ptr, int notification_id) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_set_notification(%s, %d)", p->pid,
              t->tid, cap_ptr_to_string(p, ptr, 0), notification_id);

    not_cap_set(p, ptr, notification_id);
}

void sys_reset_notification(PCB *p, TCB *t, CapPtr ptr, int notification_id) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_reset_notification(%s, %d)",
              p->pid, t->tid, cap_ptr_to_string(p, ptr, 0), notification_id);

    not_cap_reset(p, ptr, notification_id);
}

bool sys_check_notification(PCB *p, TCB *t, CapPtr ptr, int notification_id) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_check_notification(%s, %d)",
              p->pid, t->tid, cap_ptr_to_string(p, ptr, 0), notification_id);

    return not_cap_check(p, ptr, notification_id);
}

void sys_yield_thread(PCB *p, TCB *t, CapPtr ptr) {
    log_debug("线程(pid=%d, tid=%d) 调用 sys_yield_thread(%s)",
              cap_ptr_to_string(p, ptr, 0));
    tcb_cap_yield(p, ptr);
}

umb_t syscall_handler(int sysno, RegCtx *ctx, ArgumentGetter arg_getter) {
    log_debug("=====SYSCALL START=====");
    umb_t ret = 0;
    // 第0个参数一定存放Capability
    CapPtr cap;
    cap.val = arg_getter(ctx, 0);

    PCB *const p = cur_thread->pcb;
    TCB *const t = cur_thread;

    switch (sysno) {
        case SYS_EXIT:  sys_exit(p, t, cap, arg_getter(ctx, 1)); break;
        case SYS_YIELD: sys_yield(p, t, cap); break;
        case SYS_LOG:   sys_log(p, t, (const char *)arg_getter(ctx, 1)); break;
        case SYS_WRITE_SERIAL:
            ret =
                (umb_t)sys_write_serial(p, t, (const char *)arg_getter(ctx, 1));
            break;
        case SYS_FORK:   ret = sys_fork(p, t, cap).val; break;
        case SYS_GETPID: ret = sys_getpid(p, t, cap); break;
        case SYS_CREATE_THREAD:
            ret = sys_create_thread(p, t, cap, (void *)arg_getter(ctx, 1),
                                    (int)(arg_getter(ctx, 2)))
                      .val;
            break;
        case SYS_YIELD_THREAD: sys_yield_thread(p, t, cap); break;
        case SYS_WAIT_NOTIFICATION:
            ret = (sys_wait_notification(p, t, cap,
                                         (CapPtr){.val = arg_getter(ctx, 1)},
                                         (qword *)arg_getter(ctx, 2)))
                      ? 1
                      : 0;
            break;
        case SYS_WAIT_NOTIFICATION_THREAD:
            ret = (sys_wait_notification_thread(
                      p, t, cap, (CapPtr){.val = arg_getter(ctx, 1)},
                      (qword *)arg_getter(ctx, 2)))
                      ? 1
                      : 0;
            break;
        case SYS_SET_NOTIFICATION:
            sys_set_notification(p, t, cap, (int)(arg_getter(ctx, 1)));
            break;
        case SYS_RESET_NOTIFICATION:
            sys_reset_notification(p, t, cap, (int)(arg_getter(ctx, 1)));
            break;
        case SYS_CHECK_NOTIFICATION:
            ret = (sys_check_notification(p, t, cap, (int)(arg_getter(ctx, 1))))
                      ? 1
                      : 0;
            break;
        default:
            log_info("未知系统调用号: %d", sysno);
            ret = 0xFFFF'FFFF'FFFF'FFFF;
            break;
    }
    log_debug("=====SYSCALL END=====");
    return ret;
}