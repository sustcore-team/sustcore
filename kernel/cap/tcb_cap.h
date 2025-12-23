/**
 * @file tcb_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief TCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-11
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>
#include <task/task_struct.h>

// TCB能力
typedef struct {
    bool priv_unwrap;
    // 设置线程优先级的能力
    bool priv_set_priority;
    // 挂起线程的能力
    bool priv_suspend;
    // 恢复线程的能力
    bool priv_resume;
    // 终止线程的能力
    bool priv_terminate;
    // 让出CPU的能力
    bool priv_yield;
    // 等待通知的能力
    bool priv_wait_notification;
} TCBCapPriv;

/**
 * @brief 构造TCB能力
 *
 * @param p    在p内构造一个TCB能力
 * @param tcb  构造的能力指向tcb
 * @return CapPtr 能力指针
 */
CapPtr create_tcb_cap(PCB *p, TCB *tcb);

/**
 * @brief 解包TCB能力, 获得TCB指针
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @return TCB* TCB指针
 */
TCB *tcb_cap_unwrap(PCB *p, CapPtr ptr);

/**
 * @brief 将线程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 */
void tcb_cap_yield(PCB *p, CapPtr ptr);

#define TCB_CAP_START(p, ptr, fun, cap, tcb, priv, ret) \
    Capability *cap = fetch_cap(p, ptr);                \
    if (cap == nullptr) {                               \
        log_error(#fun ":指针指向的能力不存在!");       \
        return ret;                                     \
    }                                                   \
    if (cap->type != CAP_TYPE_TCB) {                    \
        log_error(#fun ":该能力不为TCB能力!");          \
        return ret;                                     \
    }                                                   \
    if (cap->cap_data == nullptr) {                     \
        log_error(#fun ":能力数据为空!");               \
        return ret;                                     \
    }                                                   \
    if (cap->cap_priv == nullptr) {                     \
        log_error(#fun ":能力权限为空!");               \
        return ret;                                     \
    }                                                   \
    TCB *tcb         = (TCB *)cap->cap_data;            \
    TCBCapPriv *priv = (TCBCapPriv *)cap->cap_priv
