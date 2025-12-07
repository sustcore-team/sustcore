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

static tid_t TIDALLOC = 1;

static inline tid_t get_current_tid(void) {
    return TIDALLOC++;
}

static inline tid_t get_current_pid(void) {
    return get_current_tid();
}

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

void terminate_tcb(TCB *t) {
    if (t == nullptr) {
        log_error("kill_tcb: 传入的TCB指针为空");
        return;
    }
    if (t->state != TS_ZOMBIE) {
        log_error(
            "terminate_tcb: 只能清理处于ZOMBIE状态的线程 (tid=%d, state=%d)",
            t->tid, t->state);
        return;
    }
    // 对t进行资源清理

    // 从线程链表中移除
    list_remove(t, THREAD_LIST(t->pcb));

    log_debug("terminate_tcb: 线程 (tid=%d) 资源清理完成", t->tid);
}

/**
 * @brief 将能力从进程的cspace中移除
 *
 * @param cap 能力
 */
void remove_from_cspace(Capability *cap) {
    PCB *p = cap->pcb;
    if (p->cap_spaces == nullptr) {
        log_error("remove_from_cspace: 进程的cap_spaces未初始化 (pid=%d)",
                  p->pid);
        return;
    }
    if (p->cap_spaces[cap->cap_ptr.cspace] == nullptr) {
        log_error(
            "remove_from_cspace: 进程的cspace未初始化 (pid=%d, cspace=%d)",
            p->pid, cap->cap_ptr.cspace);
        return;
    }
    if (p->cap_spaces[cap->cap_ptr.cspace][cap->cap_ptr.cindex] != cap) {
        log_error(
            "remove_from_cspace: 进程的cspace中对应位置的能力不匹配 (pid=%d, "
            "cspace=%d, cindex=%d)",
            p->pid, cap->cap_ptr.cspace, cap->cap_ptr.cindex);
        return;
    }
    p->cap_spaces[cap->cap_ptr.cspace][cap->cap_ptr.cindex] = nullptr;
}

/**
 * @brief 递归清除能力树
 *
 * TODO: 避免无限递归
 *
 * @param cap 能力节点
 */
void terminate_cap_tree(Capability *cap) {
    // 递归删除子节点
    Capability *child;
    foreach_list(child, CHILDREN_CAP_LIST(cap)) {
        terminate_cap_tree(child);
    }
    // 从进程能力链表中移除
    list_remove(cap, CAPABILITY_LIST(cap->pcb));
    // 释放cap_priv
    if (cap->cap_priv != nullptr) {
        kfree(cap->cap_priv);
        cap->cap_priv = nullptr;
    }

    remove_from_cspace(cap);

    // 释放能力结构体
    kfree(cap);
}

void terminate_caps(PCB *p) {
    // 遍历其所有的能力, 对每个能力遍历这个能力的派生树
    // 把树上的能力全部删除
    Capability *cap_node;
    foreach_list(cap_node, CAPABILITY_LIST(p)) {
        terminate_cap_tree(cap_node);
    }
}

void terminate_pcb(PCB *p) {
    if (p == nullptr) {
        log_error("kill_pcb: 传入的PCB指针为空");
        return;
    }
    if (p->state != TS_ZOMBIE) {
        log_error(
            "terminate_pcb: 只能清理处于ZOMBIE状态的进程 (pid=%d, state=%d)",
            p->pid, p->state);
        return;
    }
    // 清理所有的线程
    TCB *t;
    foreach_list(t, THREAD_LIST(p)) {
        terminate_tcb(t);
    }

    // 对p进行资源清理

    // 从进程链表中移除
    list_remove(p, PROC_LIST);

    // 删除其派生出的能力
    terminate_caps(p);

    log_debug("terminate_pcb: 进程 (pid=%d) 资源清理完成", p->pid);
}

void init_pcb(PCB *p, int rp_level) {
    memset(p, 0, sizeof(PCB));
    p->pid          = get_current_pid();
    p->rp_level     = rp_level;
    p->called_count = 0;
    p->priority     = 0;
    p->run_time     = 0;
    p->state        = TS_READY;

    // 加入到全局进程链表
    list_push_back(p, PROC_LIST);

    // 构造cspaces
    p->cap_spaces = (CSpace *)kmalloc(sizeof(CSpace) * PROC_CSPACES);
    memset(p->cap_spaces, 0, sizeof(CSpace) * PROC_CSPACES);
}

PCB *create_pcb(TM *tm, int rp_level, PCB *parent) {
    if (rp_level < 0 || rp_level >= RP_LEVELS) {
        log_error("new_task: 无效的RP级别 %d", rp_level);
        return nullptr;
    }

    // 初始化PCB
    PCB *p = (PCB *)kmalloc(sizeof(PCB));
    init_pcb(p, rp_level);

    // 设置基本信息
    p->tm = tm;

    list_init(THREAD_LIST(p));
    ordered_list_init(READY_THREAD_LIST(p));
    list_init(CHILDREN_TASK_LIST(p));
    list_init(CAPABILITY_LIST(p));

    // 设置父子关系
    p->parent = parent;
    if (parent != nullptr) {
        list_push_back(p, CHILDREN_TASK_LIST(parent));
    }
    return p;
}

/**
 * @brief 创建线程控制块
 *
 * @param proc 进程PCB指针
 * @param entrypoint 线程入口点
 * @param stack 线程栈指针
 * @return TCB*
 */
TCB *create_tcb(PCB *proc, int priority) {
    if (proc == nullptr) {
        log_error("create_tcb: 无效的进程指针");
        return nullptr;
    }

    // 分配TCB
    TCB *t = (TCB *)kmalloc(sizeof(TCB));
    memset(t, 0, sizeof(TCB));

    // 设置基本信息
    t->tid      = get_current_tid();
    t->priority = priority;

    // 初始化内核栈
    t->kstack = kmalloc(PAGE_SIZE);
    memset(t->kstack, 0, PAGE_SIZE);

    // 在内核栈中预留空间存放上下文
    void *stack_top  = t->kstack + PAGE_SIZE;
    stack_top       -= sizeof(RegCtx);
    t->ctx           = (RegCtx *)stack_top;
    memset(t->ctx, 0, sizeof(RegCtx));

    // 设置架构相关内容
    arch_setup_thread(t);

    // 加入到进程的线程链表
    t->pcb = proc;
    list_push_back(t, THREAD_LIST(proc));

    return t;
}

/**
 * @brief 创建线程控制块
 *
 * @param proc 进程PCB指针
 * @param entrypoint 线程入口点
 * @param stack 线程栈指针
 * @return TCB*
 */
TCB *new_thread(PCB *proc, void *entrypoint, void *stack, int priority) {
    if (entrypoint == nullptr) {
        log_error("new_thread: 无效的入口点地址");
        return nullptr;
    }

    if (stack == nullptr) {
        log_error("new_thread: 无效的栈地址");
        return nullptr;
    }

    TCB *t        = create_tcb(proc, priority);
    t->entrypoint = entrypoint;

    // 设置ip, sp
    *t->ip = entrypoint;
    *t->sp = stack;

    // 加入到进程的待命线程链表
    ordered_list_insert(t, READY_THREAD_LIST(proc));
    return t;
}

PCB *new_task(TM *tm, void *stack, void *heap, void *entrypoint, int rp_level,
              PCB *parent) {
    if (stack == nullptr) {
        log_error("new_task: 无效的栈地址");
        return nullptr;
    }

    if (heap == nullptr) {
        log_error("new_task: 无效的堆地址");
        return nullptr;
    }

    // 构造pcb
    PCB *p = create_pcb(tm, rp_level, parent);
    if (p == nullptr) {
        log_error("new_task: 无法创建PCB");
        return nullptr;
    }
    p->thread_stack_base = (void *)THREAD_STACK_BASE;

    // 初始化进程
    // 设置128MB的堆
    add_vma(p->tm, heap, 32768 * PAGE_SIZE, VMAT_HEAP);
    // 预分配64KB
    alloc_pages_for(p->tm, heap, 16, RWX_MODE_RW, true);

    // 添加主线程栈到VMA
    // 主线程栈大小64KB(从stack向下增长)
    add_vma(p->tm, stack - 16 * PAGE_SIZE, 16 * PAGE_SIZE, VMAT_STACK);
    // 预分配主线程栈顶页
    alloc_pages_for(p->tm, stack - PAGE_SIZE, 1, RWX_MODE_RW, true);
    p->main_thread    = new_thread(p, entrypoint, stack, 0);
    p->current_thread = nullptr;

    // 为当前进程构造自己的PCB能力
    CapPtr pcb_cap_ptr =
        create_pcb_cap(p, p,
                       (PCBCapPriv){.priv_unwrap        = true,
                                    .priv_derive        = true,
                                    .priv_yield         = true,
                                    .priv_exit          = true,
                                    .priv_fork          = true,
                                    .priv_getpid        = true,
                                    .priv_create_thread = true});
    // 将PCB能力传递给进程作为第一个参数
    arch_setup_argument(p->main_thread, 0, pcb_cap_ptr.val);

    // 将堆指针传递给进程作为第二个参数
    arch_setup_argument(p->main_thread, 1, (umb_t)(heap));

    // 加入到就绪队列
    if (rp_level != 3)
        list_push_back(p, RP_LIST(rp_level));
    else
        ordered_list_insert(p, RP3_LIST);

    return p;
}

TCB *fork_thread(PCB *proc, TCB *parent_thread) {
    if (proc == nullptr) {
        log_error("fork_thread: 无效的进程指针");
        return nullptr;
    }

    if (parent_thread == nullptr) {
        log_error("fork_thread: 无效的父线程指针");
        return nullptr;
    }

    // 创建新的线程
    TCB *t        = create_tcb(proc, parent_thread->priority);
    t->entrypoint = parent_thread->entrypoint;
    if (t == nullptr) {
        log_error("fork_thread: 无法创建子线程");
        return nullptr;
    }

    // 复制上下文
    memcpy(t->ctx, parent_thread->ctx, sizeof(RegCtx));
    return t;
}

PCB *fork_task(PCB *parent) {
    // 首先对内存进行操作
    TM *new_tm = setup_task_memory();
    // 逐VMA复制
    VMA *vma;
    foreach_ordered_list(vma, TM_VMA_LIST(parent->tm)) {
        clone_vma(parent->tm, vma, new_tm);
    }

    // 构造新的PCB
    PCB *p = create_pcb(new_tm, parent->rp_level, parent);

    if (p == nullptr) {
        log_error("fork_task: 无法创建子进程");
        return nullptr;
    }

    // 复制父进程的调度信息
    p->called_count = parent->called_count;
    p->priority     = parent->priority;
    p->rp1_count    = parent->rp1_count;
    p->rp2_count    = parent->rp2_count;
    p->run_time     = parent->run_time;

    // 复制父进程的主线程
    TCB *child_main_thread = fork_thread(p, parent->main_thread);
    if (child_main_thread == nullptr) {
        log_error("fork_task: 无法创建子进程主线程");
        terminate_pcb(p);
        return nullptr;
    }
    p->main_thread    = child_main_thread;
    p->current_thread = nullptr;

    // 将主线程加入到就绪线程链表
    ordered_list_insert(child_main_thread, READY_THREAD_LIST(p));

    // 对Capability的复制等交由调用者完成
    // 加入到就绪队列
    if (p->rp_level != 3)
        list_push_back(p, RP_LIST(p->rp_level));
    else
        ordered_list_insert(p, RP3_LIST);

    mem_display_mapping_layout(p->tm->pgd);
    return p;
}

/**
 * @brief 分配线程栈
 *
 * @param proc 进程PCB指针
 * @param size 线程栈大小
 *
 * @return void* 线程栈顶地址
 */
void *alloc_thread_stack(PCB *proc, size_t size) {
    if (proc == nullptr) {
        log_error("alloc_thread_stack: 无效的进程指针");
        return nullptr;
    }

    // 对齐栈大小到页边界
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // 计算栈顶地址
    void *stack_top = proc->thread_stack_base;

    // 计算栈底地址
    void *stack_bottom = stack_top - aligned_size;

    // 更新进程的线程栈基址
    proc->thread_stack_base = stack_bottom;

    // 添加VMA
    add_vma(proc->tm, stack_bottom, aligned_size, VMAT_STACK);

    // 预分配栈空间的栈顶页
    size_t num_pages = aligned_size / PAGE_SIZE;
    alloc_pages_for(proc->tm, stack_top - PAGE_SIZE, 1, RWX_MODE_RW, true);

    return stack_top;
}