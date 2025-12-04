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
#include <syscall/syscall.h>
#include <task/proc.h>
#include <sus/syscall.h>
#include <mem/alloc.h>
#include <syscall/uaccess.h>
#include <sus/boot.h>

void sys_exit(umb_t exit_code)
{
    cur_proc->state = PS_ZOMBIE;
    log_info("进程%d调用 exit 系统调用, 退出码: %lu", cur_proc->pid, exit_code);
}

void sys_yield()
{
    cur_proc->state = PS_YIELD;
}

void sys_log(const char *msg) {
    int len = ua_strlen(msg);
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
    int len = ua_strlen(msg);
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

umb_t syscall_handler(int sysno, RegCtx *ctx, ArgumentGetter arg_getter)
{
    switch(sysno) {
        case SYS_EXIT:
            sys_exit(arg_getter(ctx, 0));
            return 0;
        case SYS_YIELD:
            sys_yield();
            return 0;
        case SYS_LOG:
            sys_log((const char *)arg_getter(ctx, 0));
            return 0;
        case SYS_WRITE_SERIAL:
            int ret = sys_write_serial((const char *)arg_getter(ctx, 0));
            return ret;
        default:
            log_info("未知系统调用号: %d", sysno);
            return (umb_t)(-1);
    }
}