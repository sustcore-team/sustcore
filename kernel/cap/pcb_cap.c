/**
 * @file pcb_cap.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <cap/pcb_cap.h>
#include <cap/tcb_cap.h>
#include <mem/alloc.h>
#include <string.h>
#include <task/proc.h>
#include <task/scheduler.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

//============================================
// PCB能力权限常量
//============================================

const qword PCB_PRIV_EXIT[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0001ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword PCB_PRIV_FORK[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0002ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword PCB_PRIV_GETPID[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0004ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword PCB_PRIV_CREATE_THREAD[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0008ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword PCB_PRIV_FOREACH_CAPS[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0010ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

//============================================
// PCB能力构造相关操作
//============================================

CapPtr create_pcb_cap(PCB *p) {
    return create_cap(p, CAP_TYPE_PCB, (void *)p, CAP_ALL_PRIV, nullptr);
}

CapPtr pcb_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p,
                      qword priv[PRIVILEDGE_QWORDS]) {
    PCB_CAP_START(src_p, src_ptr, pcb_cap_derive, cap, pcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);
    (void)pcb;

    // 进行派生
    return derive_cap(dst_p, cap, priv, nullptr);
}

CapPtr pcb_cap_derive_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr,
                         qword priv[PRIVILEDGE_QWORDS]) {
    PCB_CAP_START(src_p, src_ptr, pcb_cap_derive_at, cap, pcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);
    (void)pcb;

    // 进行派生
    return derive_cap_at(dst_p, cap, priv, nullptr, dst_ptr);
}

CapPtr pcb_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p) {
    PCB_CAP_START(src_p, src_ptr, pcb_cap_clone, cap, pcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);

    (void)pcb;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return pcb_cap_derive(src_p, src_ptr, dst_p, cap->cap_priv);
}

CapPtr pcb_cap_clone_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p,
                        CapPtr dst_ptr) {
    PCB_CAP_START(src_p, src_ptr, pcb_cap_clone_at, cap, pcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);

    (void)pcb;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return pcb_cap_derive_at(src_p, src_ptr, dst_p, dst_ptr, cap->cap_priv);
}

CapPtr pcb_cap_degrade(PCB *p, CapPtr cap_ptr,
                       qword cap_priv[PRIVILEDGE_QWORDS]) {
    PCB_CAP_START(p, cap_ptr, pcb_cap_degrade, cap, pcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);
    (void)pcb;  // 未使用, 特地标记以避免编译器警告

    if (!degrade_cap(p, cap, cap_priv)) {
        return INVALID_CAP_PTR;
    }

    return cap_ptr;
}

PCB *pcb_cap_unpack(PCB *p, CapPtr cap_ptr) {
    PCB_CAP_START(p, cap_ptr, pcb_cap_unpack, cap, pcb, CAP_PRIV_UNPACK,
                  nullptr);
    return pcb;
}

//============================================
// PCB能力对象操作相关操作
//============================================

void pcb_cap_exit(PCB *p, CapPtr cap_ptr) {
    PCB_CAP_START(p, cap_ptr, pcb_cap_exit, cap, pcb, PCB_PRIV_EXIT, );

    (void)pcb;

    // TODO: 结束进程
}

PCB *pcb_cap_fork(PCB *p, CapPtr cap_ptr, CapPtr *child_cap) {
    PCB_CAP_START(p, cap_ptr, pcb_cap_fork, cap, pcb, PCB_PRIV_FORK, nullptr);
    // 进行fork操作
    PCB *fork_pcb = fork_task(pcb);
    // 为子进程创建Capability
    *child_cap    = create_pcb_cap(fork_pcb);
    return fork_pcb;
}

pid_t pcb_cap_getpid(PCB *p, CapPtr cap_ptr) {
    PCB_CAP_START(p, cap_ptr, pcb_cap_getpid, cap, pcb, PCB_PRIV_GETPID,
                  (pid_t)(-1));

    return pcb->pid;
}

CapPtr pcb_cap_create_thread(PCB *p, CapPtr cap_ptr, void *entrypoint,
                             int priority) {
    PCB_CAP_START(p, cap_ptr, pcb_cap_create_thread, cap, pcb,
                  PCB_PRIV_CREATE_THREAD, INVALID_CAP_PTR);

    // 分配线程栈
    void *stack = alloc_thread_stack(pcb, 16 * PAGE_SIZE);
    // 创建线程
    TCB *thread = new_thread(pcb, entrypoint, stack, priority);
    // 加入就绪队列
    insert_ready_thread(thread);

    // 为线程创建能力
    return create_tcb_cap(p, thread);
}