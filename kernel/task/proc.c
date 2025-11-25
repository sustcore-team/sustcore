/**
 * @file proc.c
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "proc.h"

#include <arch/riscv64/int/exception.h>
#include <basec/logger.h>
#include <mem/alloc.h>
#include <string.h>
#include <sus/boot.h>
#include <task/pid.h>

// 预分配的内核栈
__attribute__((
    aligned(4096))) static umb_t kstack_pool[POOL_SIZE][1024];  // 8KB per stack

static ProcessControlBlock pool[POOL_SIZE];
static ProcessControlBlock *cur_proc;

void proc_init(void) {
    pid_init();

    for (size_t i = 0; i < POOL_SIZE; i++) {
        pool[i].pid    = pid_alloc();
        pool[i].state  = READY;
        pool[i].kstack = (umb_t *)&kstack_pool[i];  // 指向内核栈

        memset(&pool[i].ctx, 0, sizeof(Context));

        // 初始化栈指针为栈顶 (栈向下增长)
        // 栈大小为 8192 字节 (1024 * 8)
        pool[i].ctx.sp = (umb_t)pool[i].kstack + sizeof(kstack_pool[i]);

        // 设置进程入口点（worker 函数）
        pool[i].ctx.ra = (umb_t)worker;

        // // 设置参数：s1 中存放 pid
        // pool[i].ctx.s1 = pool[i].pid;
    }
    cur_proc        = &pool[0];
    cur_proc->state = RUNNING;

    log_info("进程管理初始化完成, 共 %d 个进程", POOL_SIZE);
    for (size_t i = 0; i < POOL_SIZE; i++) {
        log_debug("进程 %d: pid=%d, kstack=0x%lx, sp=0x%lx, ra=0x%lx", i,
                  pool[i].pid, (umb_t)pool[i].kstack, pool[i].ctx.sp,
                  pool[i].ctx.ra);
    }
}

/**
 * @brief 调度器 - 选择下一个就绪进程进行切换
 *
 * 使用简单的轮转调度算法，寻找下一个状态为 READY 的进程。
 * 这是一个非阻塞式调度，只有在找到 READY 的进程时才会切换。
 */
void schedule(void) {
    ProcessControlBlock *next = NULL;

    // 从当前进程的下一个开始查找
    for (size_t i = cur_proc->pid + 1;; i++) {
        i = i % POOL_SIZE;

        ProcessControlBlock *p = &pool[i];
        if (p != cur_proc && p->state == READY) {
            next = p;
            break;
        }
    }

    // 如果没找到其他 READY 的进程，就继续运行当前进程
    if (next == NULL) {
        log_debug("调度: 无其他就绪进程，继续运行当前进程 %d (pid=%d)",
                  (int)(cur_proc - pool), cur_proc->pid);
        return;
    }

    // 更新进程状态

    ProcessControlBlock *prev = cur_proc;
    cur_proc                  = next;

    prev->state = READY;
    next->state = RUNNING;
    log_debug("调度: 从进程 %d (pid=%d) 切换到进程 %d (pid=%d)",
              (int)(prev - pool), prev->pid, (int)(next - pool), next->pid);
    // 执行上下文切换
    // __switch 会保存 prev 的被调用者保存寄存器到 &prev->ctx
    // 然后恢复 next 的被调用者保存寄存器从 &next->ctx
    __switch(&prev->ctx, &next->ctx);
}

void worker(void) {
    sti();
    // 获取当前进程的 pid
    umb_t pid = cur_proc->pid;

    log_info("进程(pid=%d) 执行中...", pid);

    while (1) {}
}