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

#include <cap/pcb_cap.h>
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

#ifdef DLOG_TASK
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

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

    // 构造cspaces
    p->cap_spaces = (CSpace *)kmalloc(sizeof(CSpace) * PROC_CSPACES);
}

PCB *new_task(TM *tm, void *stack, void *heap, void *entrypoint, int rp_level, PCB *parent)
{
    if (rp_level < 0 || rp_level >= RP_LEVELS) {
        log_error("new_task: 无效的RP级别 %d", rp_level);
        return nullptr;
    }

    if (stack == nullptr) {
        log_error("new_task: 无效的栈地址");
        return nullptr;
    }

    if (heap == nullptr) {
        log_error("new_task: 无效的堆地址");
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
    p->tm = tm;

    // 设置初始线程栈与堆的VMA

    // 设置64KB的初始栈(stack为栈顶)
    void *stack_end = stack - 16 * PAGE_SIZE;
    add_vma(p->tm, stack_end, 16 * PAGE_SIZE, VMAT_STACK);
    // 设置128MB的堆
    add_vma(p->tm, heap, 32768 * PAGE_SIZE, VMAT_HEAP);

    // 预分配4KB
    alloc_pages_for(p->tm, stack_end + 15 * PAGE_SIZE, 1, RWX_MODE_RW, true);
    alloc_pages_for(p->tm, heap, 1, RWX_MODE_RW, true);

    // 入口点
    p->entrypoint = entrypoint;

    // 设置父子关系
    p->parent = parent;
    if (parent != nullptr) {
        list_push_back(p, CHILDREN_TASK_LIST(parent));
    }

    // 分配内核栈
    log_info("为进程(PID:%d)分配内核栈: %p", p->pid, p->kstack);
    p->kstack = (umb_t *)kmalloc(PAGE_SIZE);
    memset(p->kstack, 0, PAGE_SIZE);

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
    *p->sp = (void *)((umb_t)stack);

    // 为当前进程构造自己的PCB能力
    CapPtr pcb_cap_ptr = create_pcb_cap(p, p,
                                        (PCBCapPriv){
                                            .priv_yield = true,
                                            .priv_exit  = true,
                                            .priv_fork  = true,
                                            .priv_getpid = true,
                                        });
    // 将PCB能力传递给进程作为第一个参数
    arch_setup_argument(p, 0, pcb_cap_ptr.val);

    // 将堆指针传递给进程作为第二个参数
    arch_setup_argument(p, 1, (umb_t)(heap));

    // 加入到就绪队列
    if (rp_level != 3)
        list_push_back(p, RP_LIST(rp_level));
    else
        ordered_list_insert(p, RP3_LIST);

    return p;
}

PCB *fork_task(PCB *parent) {
    // // 代码段是可以直接共享的
    // // TODO: 为数据段采取Copy-On-Write策略
    // // 目前先直接复制数据段

    // // 构造新的页表
    // MMInfo *mm = &parent->segments;
    // void *pgd  = setup_paging_table();
    // // 复制代码段映射
    // // 遍历每个页并建立映射
    // for (void *addr = mm->text_start; addr < mm->text_end;
    //      addr       = (void *)((umb_t)addr + PAGE_SIZE))
    // {
    //     // 获得每个页的物理地址与大小
    //     LargablePTEntry entry = mem_get_page(mm->pgd, addr);
    //     void *paddr           = mem_pte_dst(entry.entry);
    //     size_t pages          = PAGE_SIZE_BY_LEVEL(entry.level) / PAGE_SIZE;
    //     // 建立映射
    //     mem_maps_range_to(pgd, addr, paddr, pages, RWX_MODE_RX, true, false);
    // }

    // // 复制数据段
    // // 首先计算其页数
    // size_t data_pages =
    //     ((umb_t)mm->data_end - (umb_t)mm->data_start + PAGE_SIZE - 1) /
    //     PAGE_SIZE;
    // // 再分配这么多物理页
    // alloc_pages_for(pgd, mm->data_start, data_pages, RWX_MODE_RW, true);
    // // 复制数据
    // memcpy_u2u(pgd, mm->data_start, mm->pgd, mm->data_start,
    //            data_pages * PAGE_SIZE);

    // // 构造新进程
    // PCB *p = new_task(pgd, mm->text_start, mm->text_end, mm->data_start,
    //                   mm->data_end, mm->stack_start, mm->heap_start,
    //                   parent->entrypoint, parent->rp_level, parent);
    // if (p == nullptr) {
    //     log_error("fork_task: 无法创建子进程");
    // }

    // // 复制栈, 堆内容
    // // TODO: 其实首先应当拓展预分配的栈与堆的空间
    // // 但目前先默认预分配的空间足够用

    // // 复制栈内容
    // size_t stack_pages =
    //     ((umb_t)parent->segments.stack_start - (umb_t)parent->segments.stack_end) /
    //     PAGE_SIZE;
    // memcpy_u2u(pgd, p->segments.stack_end, parent->segments.pgd,
    //            parent->segments.stack_end, stack_pages * PAGE_SIZE);

    // // 复制堆内容
    // size_t heap_pages =
    //     ((umb_t)parent->segments.heap_end - (umb_t)parent->segments.heap_start) /
    //     PAGE_SIZE;
    // memcpy_u2u(pgd, p->segments.heap_start, parent->segments.pgd,
    //            parent->segments.heap_start, heap_pages * PAGE_SIZE);

    // // 复制上下文
    // memcpy(p->ctx, parent->ctx, sizeof(RegCtx));

    // mem_display_mapping_layout(pgd);
    // return p;
    return nullptr;
}