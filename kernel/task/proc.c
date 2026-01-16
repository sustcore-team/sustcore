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

#include <cap/not_cap.h>
#include <cap/pcb_cap.h>
#include <cap/tcb_cap.h>
#include <mem/alloc.h>
#include <mem/kmem.h>
#include <mem/buddy.h>
#include <mem/vmm.h>
#include <string.h>
#include <sus/boot.h>
#include <sus/ctx.h>
#include <sus/list_helper.h>
#include <sus/paging.h>
#include <sus/symbols.h>
#include <task/scheduler.h>

#ifdef DLOG_TASK
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

PCB *proc_list_head;
PCB *proc_list_tail;
PCB empty_proc;
TCB empty_thread;

WaitingTCB *waiting_list_head;
WaitingTCB *waiting_list_tail;
TCB *rp_list_heads[RP_LEVELS];
TCB *rp_list_tails[RP_LEVELS];
TCB *cur_thread;

// PID, TID 的分配
static tid_t TIDALLOC = 1;

static inline tid_t get_current_tid(void) {
    return TIDALLOC++;
}

static inline tid_t get_current_pid(void) {
    return get_current_tid();
}

const char *thread_state_to_string(ThreadState state) {
    switch (state) {
        case TS_EMPTY:     return "EMPTY";
        case TS_READY:     return "READY";
        case TS_RUNNING:   return "RUNNING";
        case TS_BLOCKED:   return "BLOCKED";
        case TS_SUSPENDED: return "SUSPENDED";
        case TS_WAITING:   return "WAITING";
        case TS_ZOMBIE:    return "ZOMBIE";
        case TS_UNUSED:    return "UNUSED";
        case TS_YIELD:     return "YIELD";
        default:           return "UNKNOWN";
    }
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
    cur_thread = nullptr;
}

// 初始化PCB块
PCB *init_pcb(TM *tm, int rp_level, PCB *parent, void *thread_stack_base,
              int priority) {
    if (rp_level < 0 || rp_level >= RP_LEVELS) {
        log_error("new_task: 无效的RP级别 %d", rp_level);
        return nullptr;
    }

    PCB *p = (PCB *)kmalloc(sizeof(PCB));
    memset(p, 0, sizeof(PCB));
    p->pid      = get_current_pid();
    p->rp_level = rp_level;

    p->thread_stack_base = thread_stack_base;

    // 加入到全局进程链表
    list_push_back(p, PROC_LIST);

    // 构造cspaces
    p->cap_spaces = (CSpace *)kmalloc(sizeof(CSpace) * PROC_CSPACES);
    memset(p->cap_spaces, 0, sizeof(CSpace) * PROC_CSPACES);

    // 设置TASK Memory
    p->tm = tm;

    // 初始化各个链表
    list_init(THREAD_LIST(p));
    list_init(CHILDREN_TASK_LIST(p));
    list_init(CAPABILITY_LIST(p));

    // 设置父子关系
    p->parent = parent;
    if (parent != nullptr) {
        list_push_back(p, CHILDREN_TASK_LIST(parent));
    }

    return p;
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

    // 设置128MB的堆
    add_vma(tm, heap, 32768 * PAGE_SIZE, VMAT_HEAP);
    // 预分配64KB
    alloc_pages_for(tm, heap, 16, RWX_MODE_RW, true);

    // 添加主线程栈到VMA
    // 主线程栈大小64KB(从stack向下增长)
    add_vma(tm, stack - 16 * PAGE_SIZE, 16 * PAGE_SIZE, VMAT_STACK);
    // 预分配主线程栈顶页
    alloc_pages_for(tm, stack - PAGE_SIZE, 1, RWX_MODE_RW, true);

    // 构造pcb
    PCB *p = init_pcb(tm, rp_level, parent, THREAD_STACK_BASE, 0);
    if (p == nullptr) {
        log_error("new_task: 无法创建PCB");
        return nullptr;
    }

    // 构造主线程
    TCB *main_thread = new_thread(p, entrypoint, stack, MAIN_THREAD_PRIORITY);
    if (main_thread == nullptr) {
        log_error("new_task: 无法创建主线程");
        terminate_pcb(p);
        return nullptr;
    }

    p->main_thread = main_thread;

    // 为当前进程构造自己的PCB能力
    CapIdx pcb_idx   = create_pcb_cap(p);
    // 为当前进程构造主线程能力
    CapIdx main_tcb_ptr  = create_tcb_cap(p, main_thread);
    // 为当前进程构造Notification能力
    CapIdx notif_idx = create_notification_cap(p);

    // 三个参数:
    // PCB能力索引, 进程堆指针, 主线程能力, 初始Notification能力索引
    arch_setup_argument(p->main_thread, 0, pcb_idx.val);
    arch_setup_argument(p->main_thread, 1, (umb_t)(heap));
    arch_setup_argument(p->main_thread, 2, main_tcb_ptr.val);
    arch_setup_argument(p->main_thread, 3, notif_idx.val);

    // 将主线程加入到就绪线程链表
    insert_ready_thread(main_thread);

    return p;
}

static void fork_caps(PCB *parent, PCB *child) {
    Capability *cap;
    // 遍历父进程的能力链表
    foreach_list(cap, CAPABILITY_LIST(parent)) {
        // CapIdx idx = cap->idx;
        // 克隆能力到子进程, 且位置相同
        // cap_clone_at(parent, ptr, child, ptr);
    }
}

PCB *fork_task(PCB *parent) {
    if (parent == nullptr) {
        log_error("fork_task: 无效的父进程指针");
        return nullptr;
    }

    // 判断是否为单线程进程
    if (parent->threads_head != parent->threads_tail) {
        log_error("fork_task: 仅支持单线程进程的fork");
        return nullptr;
    }

    // 复制父进程的内存空间
    TM *new_tm = setup_task_memory();
    // 逐VMA复制
    VMA *vma;
    foreach_ordered_list(vma, TM_VMA_LIST(parent->tm)) {
        clone_vma(parent->tm, vma, new_tm);
    }

    // 构造新的PCB
    PCB *child = init_pcb(new_tm, parent->rp_level, parent,
                      parent->thread_stack_base, parent->main_thread->priority);

    if (child == nullptr) {
        log_error("fork_task: 无法创建子进程");
        return nullptr;
    }

    // 复制父进程的主线程
    TCB *child_main_thread = fork_thread(child, parent->main_thread);
    if (child_main_thread == nullptr) {
        log_error("fork_task: 无法创建子进程主线程");
        terminate_pcb(child);
        return nullptr;
    }
    child->main_thread = child_main_thread;

    insert_ready_thread(child->main_thread);

    // 遍历父进程的能力, 派生新的能力到子进程
    fork_caps(parent, child);

    // 显示新进程的内存映射布局
    mem_display_mapping_layout(child->tm->pgd);
    return child;
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

    // 计算栈地址
    void *stack_top    = proc->thread_stack_base;
    void *stack_bottom = stack_top - aligned_size;

    // 更新进程的线程栈基址
    proc->thread_stack_base = stack_bottom;

    // 添加VMA
    add_vma(proc->tm, stack_bottom, aligned_size, VMAT_STACK);

    // 预分配栈空间的栈顶页
    alloc_pages_for(proc->tm, stack_top - PAGE_SIZE, 1, RWX_MODE_RW, true);

    return stack_top;
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
    t->rp1_count = 0;
    t->rp2_count = 0;
    t->run_time  = 0;

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
    *t->ip   = entrypoint;
    *t->sp   = stack;
    t->state = TS_READY;
    return t;
}

TCB *fork_thread(PCB *p, TCB *parent_thread) {
    if (p == nullptr) {
        log_error("fork_thread: 无效的进程指针");
        return nullptr;
    }

    if (parent_thread == nullptr) {
        log_error("fork_thread: 无效的父线程指针");
        return nullptr;
    }

    // 创建新的线程
    TCB *t        = create_tcb(p, parent_thread->priority);
    t->entrypoint = parent_thread->entrypoint;
    if (t == nullptr) {
        log_error("fork_thread: 无法创建子线程");
        return nullptr;
    }

    // 复制上下文
    memcpy(t->ctx, parent_thread->ctx, sizeof(RegCtx));
    return t;
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
    if (p->cap_spaces[cap->idx.cspace] == nullptr) {
        log_error(
            "remove_from_cspace: 进程的cspace未初始化 (pid=%d, cspace=%d)",
            p->pid, cap->idx.cspace);
        return;
    }
    if (p->cap_spaces[cap->idx.cspace][cap->idx.cindex] != cap) {
        log_error(
            "remove_from_cspace: 进程的cspace中对应位置的能力不匹配 (pid=%d, "
            "cspace=%d, cindex=%d)",
            p->pid, cap->idx.cspace, cap->idx.cindex);
        return;
    }
    p->cap_spaces[cap->idx.cspace][cap->idx.cindex] = nullptr;
}

// TODO: 正式实现下列的terminate函数

/**
 * @brief 递归清除能力树
 *
 * TODO: 避免无限递归
 *
 * @param cap 能力节点
 */
void terminate_cap_tree(Capability *cap) {
    // TODO: 实现该函数
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