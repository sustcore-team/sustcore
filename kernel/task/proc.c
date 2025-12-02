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
#include <mem/kmem.h>
#include <mem/pmm.h>
#include <string.h>
#include <sus/boot.h>
#include <sus/ctx.h>
#include <sus/paging.h>
#include <sus/symbols.h>
#include <task/pid.h>

PCB *cur_proc;

PCB *proc_pool[NPROG];
PCB *init_proc = NULL;

/**
 * @brief 初始化进程池
 *
 */
void proc_init(void) {
    PCB *p;
    for (size_t i = 0; i < NPROG; i++) {
        p = (PCB *)kmalloc(sizeof(PCB));
        memset(p, 0, sizeof(PCB));

        p->pid   = get_current_pid();
        p->state = READY;
        log_info("initialize kstack for proc %d", i);
        p->kstack = (umb_t *)kmalloc(PAGE_SIZE);

        log_info("initialize ctx for proc %d", i);
        umb_t *stack_top  = (umb_t *)((umb_t)p->kstack + PAGE_SIZE);
        stack_top        -= sizeof(RegCtx) / sizeof(umb_t);
        p->ctx            = (RegCtx *)stack_top;
        memset(p->ctx, 0, sizeof(RegCtx));

        // 1. 分配一页作为用户栈 (alloc_page 返回物理地址)
        umb_t user_stack_pa = (umb_t)alloc_page();

        // 2. 获取内核虚拟地址以便清空栈内容
        void *user_stack_kva = PA2KPA(user_stack_pa);
        memset(user_stack_kva, 0, PAGE_SIZE);

        // 3. 栈向下增长，所以 SP 指向页面末尾
        p->ctx->regs[1] = user_stack_pa + PAGE_SIZE;

        log_info("proc %d user stack: PA=0x%lx, SP=0x%lx", i, user_stack_pa,
                 p->ctx->regs[1]);

        // ra
        p->ctx->regs[0]      = 0;
        // 代码运行在 U-Mode
        p->ctx->sstatus.spp  = 0;
        // 用户进程应该开启中断
        p->ctx->sstatus.spie = 1;

        proc_pool[i] = p;
    }

    for (size_t i = 0; i < 2; i++) {
        if (p == NULL) {
            log_error("proc_pool[%d] 未初始化", i);
            continue;
        }

        p            = proc_pool[i];
        p->ctx->sepc = KA2PA((umb_t)&s_ptest1);
        log_debug("进程 %d: 用户代码加载到 0x%lx", i, KA2PA((umb_t)&s_ptest1));
    }

    cur_proc = NULL;
    log_info("进程管理初始化完成, 等待第一次时钟中断调度...");
}

/**
 * @brief 分配一个进程控制块
 *
 * @return ProcessControlBlock*
 */
PCB *alloc_proc(void) {
    for (size_t i = 0; i < NPROG; i++) {
        if (proc_pool[i]->state == UNUSED) {
            proc_pool[i]->state = READY;
            if (!init_proc) {
                init_proc = proc_pool[i];
            }

            return proc_pool[i];
        }
    }
    return NULL;
}

/**
 * @brief 释放并清空一个进程控制块
 *
 * @param proc
 * @param pid
 */
void dealloc_proc(PCB *proc, umb_t pid) {
    if (proc_pool[pid] == proc) {
        memset(proc_pool[pid], 0, sizeof(PCB));
        proc_pool[pid]->pid   = pid;
        proc_pool[pid]->state = UNUSED;
        return;
    }
}

/**
 * @brief 调度器 - 选择下一个就绪进程进行切换
 *
 * 使用简单的轮转调度算法，寻找下一个状态为 READY 的进程。
 * 这是一个非阻塞式调度，只有在找到 READY 的进程时才会切换。
 */
RegCtx *schedule(RegCtx *old) {
    PCB *next = NULL;
    log_debug("schedule: 当前进程 pid=%d", cur_proc ? cur_proc->pid : -1);
    log_debug("current state: %d", cur_proc ? cur_proc->state : -1);
    if (cur_proc == NULL) {
        for (size_t i = 0; i < NPROG; i++) {
            if (proc_pool[i] && proc_pool[i]->state == READY) {
                next = proc_pool[i];
                break;
            }
        }

        if (next) {
            cur_proc    = next;
            next->state = RUNNING;
            log_info("首次调度: 启动进程 (pid=%d)", next->pid);
            // 直接返回新进程的上下文，丢弃 old (内核引导线程的上下文)
            log_debug("schedule: 进入进程 pid=%d, sepc 0x%x", next->pid,
                      next->ctx->sepc);
            return next->ctx;
        } else {
            return old;
        }
    }

    cur_proc->ctx = old;

    for (size_t i = 0; i < NPROG; i++) {
        PCB *p = proc_pool[i];
        if (p != cur_proc && p->state == READY) {
            next = p;
            break;
        }
    }

    // 如果没找到其他 READY 的进程，就继续运行当前进程
    if (next == NULL) {
        if (cur_proc->state == ZOMBIE) {
            log_info("所有进程运行完毕");
            while (1) {}
        }
        log_debug("调度: 无其他就绪进程，继续运行当前进程(pid=%d)",
                  cur_proc->pid);

        return old;
    }

    // 更新进程状态
    PCB *prev = cur_proc;
    cur_proc  = next;

    if (prev->state != ZOMBIE) {
        prev->state = READY;
    }

    next->state = RUNNING;

    log_debug("调度: 从进程 (pid=%d) 切换到进程 (pid=%d)", prev->pid,
              next->pid);

    return next->ctx;
}

RegCtx *exit_current_task() {
    if (cur_proc == NULL) {
        log_error("exit_current_task: 当前没有运行的进程");
        return NULL;
    }
    cur_proc->state = ZOMBIE;

    RegCtx *next_ctx = schedule(cur_proc->ctx);

    return next_ctx;
}

__attribute__((section(".ptest1"), used)) void worker(void) {
    // syscall(SYS_EXIT, 0);
    asm volatile(
        "mv a7, %0\n"
        "mv a0, %1\n"
        "ecall\n"
        :
        : "r"(93), "r"(0)
        : "a0", "a7");
    // 纯净的循环，不依赖任何内核数据
    while (1) {
        // 简单的忙等待，防止被编译器优化掉
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void cal_prime(void) {
    int n = 1000000;
    for (size_t j = 1; j < n; j++) {
        if (j <= 1 || ((j > 2) && (j % 2 == 0)))
            continue;

        int cnt = 0;

        if (j == 2) {
            kprintf("PRIME: %d is prime\n", j);
            continue;
        } else {
            for (int i = 3; i * i <= j; i += 2) {
                if (j % i == 0)
                    cnt++;
            }
            if (cnt > 0)
                continue;
            // else n is prime
            else
                kprintf("PRIME: %d is prime\n", j);
        }
    }
}