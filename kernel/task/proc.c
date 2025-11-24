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

#include <basec/logger.h>
#include <mem/alloc.h>
#include <string.h>
#include <task/pid.h>


ProcessControlBlock *pool;
ProcessControlBlock *cur_proc;
// __attribute__((aligned(4096))) umb_t ustack[POOL_SIZE][4096];

void proc_init(umb_t *ustack, ProcessControlBlock *pool) {
    pid_init();

    for (size_t i = 0; i < POOL_SIZE; i++) {
        pool[i].pid   = pid_alloc();
        pool[i].state = READY;
        pool[i].stack = (umb_t *)&ustack[i];  // 使用预分配的栈
        memset(&pool[i].ctx, 0, sizeof(Context));

        // 初始化栈指针为栈顶 (栈向下增长)
        // 栈大小为 4096 字节 (4096 / 8 = 512 个 64-bit 值)
        pool[i].ctx.sp = (umb_t)pool[i].stack + 4096;
    }
    cur_proc        = &pool[0];
    cur_proc->state = RUNNING;

    log_info("进程管理初始化完成, 共 %d 个进程", POOL_SIZE);
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
    for (size_t i = 0; i < POOL_SIZE; i++) {
        ProcessControlBlock *p = &pool[i];
        if (p != cur_proc && p->state == READY) {
            next = p;
            break;
        }
    }

    // 如果没找到其他 READY 的进程，就继续运行当前进程
    if (next == NULL) {
        return;
    }

    // 更新进程状态
    log_debug("调度: 从进程 %d (pid=%d) 切换到进程 %d (pid=%d)",
              (int)(cur_proc - pool), cur_proc->pid, (int)(next - pool),
              next->pid);

    ProcessControlBlock *prev = cur_proc;
    cur_proc                  = next;

    prev->state = READY;
    next->state = RUNNING;

    // 执行上下文切换
    // __switch 会保存 prev 的被调用者保存寄存器到 &prev->ctx
    // 然后恢复 next 的被调用者保存寄存器从 &next->ctx
    __switch(&prev->ctx, &next->ctx);
}
