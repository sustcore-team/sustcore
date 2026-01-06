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

CapPtr create_pcb_cap(PCB *p) {
    static const PCBCapPriv PCB_ALL_PRIV = {
        .priv_unwrap        = true,
        .priv_derive        = true,
        .priv_exit          = true,
        .priv_fork          = true,
        .priv_getpid        = true,
        .priv_create_thread = true,
    };

    PCBCapPriv *priv = (PCBCapPriv *)kmalloc(sizeof(PCBCapPriv));
    memcpy(priv, &PCB_ALL_PRIV, sizeof(PCBCapPriv));
    return create_cap(p, CAP_TYPE_PCB, (void *)p, (void *)priv);
}

PCB *pcb_cap_unwrap(PCB *p, CapPtr ptr) {
    PCB_CAP_START(p, ptr, pcb_cap_unwrap, cap, pcb, priv, nullptr);

    // 是否有对应权限
    if (!priv->priv_unwrap) {
        log_error("该能力不具有unwrap权限!");
        return nullptr;
    }

    return pcb;
}

void pcb_cap_exit(PCB *p, CapPtr ptr) {
    PCB_CAP_START(p, ptr, pcb_cap_exit, cap, pcb, priv, );

    (void)pcb;

    // 是否有对应权限
    if (!priv->priv_exit) {
        log_error("该能力不具有exit权限!");
        return;
    }

    // TODO: 结束进程
}

PCB *pcb_cap_fork(PCB *p, CapPtr ptr, CapPtr *child_cap) {
    PCB_CAP_START(p, ptr, pcb_cap_fork, cap, pcb, priv, nullptr);

    // 是否有对应权限
    if (!priv->priv_fork) {
        log_error("该能力不具有fork权限!");
        return nullptr;
    }

    // 进行fork操作
    PCB *fork_pcb = fork_task(pcb);
    // 为子进程创建Capability
    *child_cap    = create_pcb_cap(fork_pcb);
    return fork_pcb;
}

pid_t pcb_cap_getpid(PCB *p, CapPtr ptr) {
    PCB_CAP_START(p, ptr, pcb_cap_getpid, cap, pcb, priv, (pid_t)(-1));

    // 是否有对应权限
    if (!priv->priv_getpid) {
        log_error("该能力不具有getpid权限!");
        return (pid_t)(-1);
    }

    return pcb->pid;
}

CapPtr pcb_cap_create_thread(PCB *p, CapPtr ptr, void *entrypoint,
                             int priority) {
    PCB_CAP_START(p, ptr, pcb_cap_create_thread, cap, pcb, priv,
                  INVALID_CAP_PTR);

    // 是否有对应权限
    if (!priv->priv_create_thread) {
        log_error("该能力不具有create_thread权限!");
        return INVALID_CAP_PTR;
    }

    // 分配线程栈
    void *stack = alloc_thread_stack(pcb, 16 * PAGE_SIZE);
    // 创建线程
    TCB *thread = new_thread(pcb, entrypoint, stack, priority);
    // 加入就绪队列
    insert_ready_thread(thread);

    // 为线程创建能力
    return create_tcb_cap(p, thread);
}

CapPtr pcb_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p, PCBCapPriv priv) {
    PCB_CAP_START(src_p, src_ptr, pcb_cap_derive, cap, pcb, old_priv,
                  INVALID_CAP_PTR);

    (void)pcb;  // 未使用, 特地标记以避免编译器警告

    // 首先检查是否有派生权限
    if (!old_priv->priv_derive) {
        log_error("该能力不具有derive权限!");
        return INVALID_CAP_PTR;
    }

    // 检查新权限是否有效
    if (!BOOL_IMPLIES(priv.priv_unwrap, old_priv->priv_unwrap) ||
        !BOOL_IMPLIES(priv.priv_exit, old_priv->priv_exit) ||
        !BOOL_IMPLIES(priv.priv_fork, old_priv->priv_fork) ||
        !BOOL_IMPLIES(priv.priv_getpid, old_priv->priv_getpid) ||
        !BOOL_IMPLIES(priv.priv_create_thread, old_priv->priv_create_thread))
    {
        log_error("派生的权限无效!");
        return INVALID_CAP_PTR;
    }

    // 进行派生
    PCBCapPriv *priv_ptr = (PCBCapPriv *)kmalloc(sizeof(PCBCapPriv));
    memcpy(priv_ptr, &priv, sizeof(PCBCapPriv));
    return derive_cap(dst_p, cap, (void *)priv_ptr);
}

CapPtr pcb_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p) {
    PCB_CAP_START(src_p, src_ptr, pcb_cap_clone, cap, pcb, old_priv,
                  INVALID_CAP_PTR);

    (void)pcb;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return pcb_cap_derive(src_p, src_ptr, dst_p, *old_priv);
}