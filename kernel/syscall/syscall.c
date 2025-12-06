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
#include <cap/pcb_cap.h>
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

CapPtr sys_fork(CapPtr ptr) {
    PCB *child = pcb_cap_fork(cur_proc, ptr);
    if (child == nullptr) {
        return INVALID_CAP_PTR;
    }
    // 对子进程, 设置副返回值为0
    // 主返回值为子进程的Capability(在构造task时已设置)
    arch_setup_argument(child, 1, 0);
    // 主进程的副返回值为子进程的PID
    arch_setup_argument(cur_proc, 1, (umb_t)(child->pid));
    // 移动子进程的pc
    // TODO: 这里假设每条指令长度为4字节
    // 应当通过架构相关方法获得
    *(child->ip) += 4;
    // 返回子进程的能力
    // 父进程对子进程享有全部权限
    return create_pcb_cap(cur_proc, child,
                          (PCBCapPriv){.priv_yield  = true,
                                       .priv_exit   = true,
                                       .priv_fork   = true,
                                       .priv_getpid = true});
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
        default: log_info("未知系统调用号: %d", sysno); return (umb_t)(-1);
    }
}