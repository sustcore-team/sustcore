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
#include <task/proc.h>
#include <mem/alloc.h>
#include <string.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

CapPtr create_pcb_cap(PCB *p, PCB *pcb, PCBCapPriv priv) {
    PCBCapPriv *priv_ptr = (PCBCapPriv *)kmalloc(sizeof(PCBCapPriv));
    memcpy(priv_ptr, &priv, sizeof(PCBCapPriv));
    return create_cap(p, CAP_TYPE_PCB, (void *)pcb, (void *)priv_ptr);
}

#define PCB_CAP_START(p, ptr, fun, cap, pcb, priv, ret) \
    Capability *cap = fetch_cap(p, ptr);           \
    if (cap == nullptr) {                          \
        log_error(#fun ":指针指向的能力不存在!");  \
        return ret;                                \
    }                                              \
    if (cap->type != CAP_TYPE_PCB) {               \
        log_error(#fun ":该能力不为PCB能力!");     \
        return ret;                                \
    }                                              \
    if (cap->cap_data == nullptr) {                \
        log_error(#fun ":能力数据为空!");          \
        return ret;                                \
    }                                              \
    if (cap->cap_priv == nullptr) {                \
        log_error(#fun ":能力权限为空!");          \
        return ret;                                \
    }                                              \
    PCB *pcb         = (PCB *)cap->cap_data;       \
    PCBCapPriv *priv = (PCBCapPriv *)cap->cap_priv

void pcb_cap_yield(PCB *p, CapPtr ptr) {
    PCB_CAP_START(p, ptr, pcb_cap_yield, cap, pcb, priv, );

    // 是否有对应权限
    if (!priv->priv_yield) {
        log_error("该能力不具有yield权限!");
        return;
    }

    // 让渡进程
    pcb->state = PS_YIELD;
}

void pcb_cap_exit(PCB *p, CapPtr ptr) {
    PCB_CAP_START(p, ptr, pcb_cap_exit, cap, pcb, priv, );

    // 是否有对应权限
    if (!priv->priv_exit) {
        log_error("该能力不具有exit权限!");
        return;
    }

    // 结束进程
    pcb->state = PS_ZOMBIE;
}

PCB *pcb_cap_fork(PCB *p, CapPtr ptr) {
    PCB_CAP_START(p, ptr, pcb_cap_fork, cap, pcb, priv, nullptr);

    // 是否有对应权限
    if (!priv->priv_fork) {
        log_error("该能力不具有fork权限!");
        return nullptr;
    }

    // 开始fork操作
    PCB *fork_pcb = fork_task(pcb);
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