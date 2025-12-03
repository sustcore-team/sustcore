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
#include <mem/vmm.h>
#include <string.h>
#include <sus/boot.h>
#include <sus/ctx.h>
#include <sus/list_helper.h>
#include <sus/paging.h>
#include <sus/symbols.h>

PCB *proc_list_head;
PCB *proc_list_tail;

PCB *rp_list_heads[RP_LEVELS];
PCB *rp_list_tails[RP_LEVELS];

PCB *cur_proc;

PCB empty_proc;

/**
 * @brief 初始化进程池
 *
 */
void proc_init(void) {
    // 设置链表头
    list_init(PROC_LIST);
    list_init(RP_LIST(0));
    list_init(RP_LIST(1));
    list_init(RP_LIST(2));
    // 设定rp3为有序链表
    ordered_list_init(RP3_LIST);

    // 将当前进程设为空
    cur_proc = nullptr;

    // 设置空闲进程
    // PID 0 保留给空进程
    memset(&empty_proc, 0, sizeof(PCB));
}

void terminate_pcb(PCB *p) {
    if (p == nullptr) {
        log_error("kill_pcb: 传入的PCB指针为空");
        return;
    }
    if (p->state != PS_ZOMBIE) {
        log_error(
            "terminate_pcb: 只能清理处于ZOMBIE状态的进程 (pid=%d, state=%d)",
            p->pid, p->state);
        return;
    }
    // 对p进行资源清理

    // 清空内核栈
    if (p->kstack != nullptr) {
        kfree(p->kstack);
        p->kstack = nullptr;
    }

    // 从进程链表中移除
    list_remove(p, PROC_LIST);

    log_debug("terminate_pcb: 进程 (pid=%d) 资源清理完成", p->pid);
}

void init_pcb(PCB *p, int rp_level) {
    memset(p, 0, sizeof(PCB));
    p->pid          = get_current_pid();
    p->rp_level     = rp_level;
    p->called_count = 0;
    p->priority     = 0;
    p->run_time     = 0;
    p->state        = PS_READY;

    // 加入到全局进程链表
    list_push_back(p, PROC_LIST);
}

PCB *new_task(void *pgd, void *code_start, void *code_end, void *data_start,
              void *data_end, void *stack_start, void *heap_start,
              void *entrypoint, int rp_level, PCB *parent) {
    if (rp_level < 0 || rp_level >= RP_LEVELS) {
        log_error("new_task: 无效的RP级别 %d", rp_level);
        return nullptr;
    }

    if (code_start >= code_end) {
        log_error("new_task: 无效的代码段地址");
        return nullptr;
    }

    if (data_start >= data_end) {
        log_error("new_task: 无效的数据段地址");
        return nullptr;
    }

    if (stack_start == nullptr) {
        log_error("new_task: 无效的栈段起始地址");
        return nullptr;
    }

    if (heap_start == nullptr) {
        log_error("new_task: 无效的堆段起始地址");
        return nullptr;
    }

    if (entrypoint == nullptr) {
        log_error("new_task: 无效的进程入口点");
        return nullptr;
    }

    // 初始化PCB
    PCB *p = (PCB *)kmalloc(sizeof(PCB));
    init_pcb(p, rp_level);

    // 设置基本信息
    p->segments.pgd        = pgd;
    p->segments.code_start = code_start;
    p->segments.code_end   = code_end;
    p->segments.data_start = data_start;
    p->segments.data_end   = data_end;

    // 预分配栈和堆
    const int PREALLOC_PAGES = 4;

    p->segments.stack_start = stack_start;
    void *stack_end = (void *)((umb_t)stack_start - PREALLOC_PAGES * PAGE_SIZE);
    alloc_pages_for(pgd, stack_end, PREALLOC_PAGES, RWX_MODE_RW, true);
    p->segments.stack_end = stack_end;

    p->segments.heap_start = heap_start;
    alloc_pages_for(pgd, heap_start, PREALLOC_PAGES, RWX_MODE_RW, true);
    p->segments.heap_end =
        (void *)((umb_t)heap_start + PREALLOC_PAGES * PAGE_SIZE);

    // 入口点
    p->entrypoint = entrypoint;

    // 设置父子关系
    p->parent = parent;
    if (parent != nullptr) {
        list_push_back(p, CHILDREN_LIST(parent));
    }

    // 分配内核栈
    log_info("为进程(PID:%d)分配内核栈: %p", p->pid, p->kstack);
    p->kstack = (umb_t *)kmalloc(PAGE_SIZE);

    log_info("为进程(PID:%d)初始化上下文", p->pid);
    // 留出空间存放上下文
    umb_t *stack_top  = (umb_t *)((umb_t)p->kstack + PAGE_SIZE);
    stack_top        -= sizeof(RegCtx) / sizeof(umb_t);
    p->ctx            = (RegCtx *)stack_top;
    memset(p->ctx, 0, sizeof(RegCtx));

    // 架构相关设置
    arch_setup_proc(p);

    // ip, sp寄存器
    *p->ip = entrypoint;
    *p->sp = (void *)((umb_t)stack_start);

    // 将堆指针传递给进程作为第一个参数
    arch_setup_argument(p, 0, (umb_t)(p->segments.heap_start));

    // 加入到就绪队列
    if (rp_level != 3)
        list_push_back(p, RP_LIST(rp_level));
    else
        ordered_list_insert(p, RP3_LIST);

    return p;
}