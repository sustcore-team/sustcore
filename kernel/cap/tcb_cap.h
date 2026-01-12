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

// 设置线程优先级权限
extern const qword TCB_PRIV_SET_PRIORITY[PRIVILEDGE_QWORDS];
// 挂起线程权限
extern const qword TCB_PRIV_SUSPEND[PRIVILEDGE_QWORDS];
// 恢复线程权限
extern const qword TCB_PRIV_RESUME[PRIVILEDGE_QWORDS];
// 终止线程权限
extern const qword TCB_PRIV_TERMINATE[PRIVILEDGE_QWORDS];
// 线程yield权限
extern const qword TCB_PRIV_YIELD[PRIVILEDGE_QWORDS];
// 线程等待通知权限
extern const qword TCB_PRIV_WAIT_NOTIFICATION[PRIVILEDGE_QWORDS];

/**
 * @brief 构造TCB能力
 *
 * @param p    在p内构造一个TCB能力
 * @param tcb  构造的能力指向tcb
 * @return CapPtr 能力指针
 */
CapPtr create_tcb_cap(PCB *p, TCB *tcb);

/**
 * @brief 从源进程的源能力派生一个新的TCB能力到目标进程
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr tcb_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p,
                      qword priv[PRIVILEDGE_QWORDS]);

/**
 * @brief 从源进程的源能力派生一个新的TCB能力到目标进程的指定位置
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 目标能力指针位置
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr tcb_cap_derive_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr,
                         qword priv[PRIVILEDGE_QWORDS]);

/**
 * @brief 从源进程的源能力克隆一个新的TCB能力到目标进程
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @return CapPtr 新的能力指针
 */
CapPtr tcb_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p);

/**
 * @brief 从源进程的源能力克隆一个新的TCB能力到目标进程的指定位置
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 目标能力指针位置
 * @return CapPtr 新的能力指针
 */
CapPtr tcb_cap_clone_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr);

/**
 * @brief 将能力降级为更低权限的能力
 *
 * @param p 当前进程PCB指针
 * @param cap_ptr 能力指针
 * @param cap_priv 新的能力权限
 * @return CapPtr 降级后的能力指针(和cap_ptr相同)
 */
CapPtr tcb_cap_degrade(PCB *p, CapPtr cap_ptr,
                       qword cap_priv[PRIVILEDGE_QWORDS]);

/**
 * @brief 解包TCB能力, 获得TCB指针
 *
 * @param p 当前进程PCB指针
 * @param cap_ptr 能力指针
 * @return TCB* TCB指针
 */
TCB *tcb_cap_unpack(PCB *p, CapPtr cap_ptr);

//===========

/**
 * @brief 将线程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param cap_ptr 能力指针
 */
void tcb_cap_yield(PCB *p, CapPtr cap_ptr);

#define TCB_CAP_START(proc, cap_ptr, func_name, cap, tcb, priv_check, ret_val) \
    Capability *cap = fetch_cap(proc, cap_ptr);                                \
    if (cap == nullptr) {                                                      \
        log_error(#func_name ":指针指向的能力不存在!");                        \
        return ret_val;                                                        \
    }                                                                          \
    if (cap->type != CAP_TYPE_TCB) {                                           \
        log_error(#func_name ":该能力不为TCB能力!");                           \
        return ret_val;                                                        \
    }                                                                          \
    if (cap->cap_data == nullptr) {                                            \
        log_error(#func_name ":能力数据为空!");                                \
        return ret_val;                                                        \
    }                                                                          \
    if (cap->cap_priv == nullptr) {                                            \
        log_error(#func_name ":能力权限为空!");                                \
        return ret_val;                                                        \
    }                                                                          \
    TCB *tcb = (TCB *)cap->cap_data;                                           \
    if (!derivable(cap->cap_priv, priv_check)) {                               \
        log_error(#func_name ":能力权限不足!");                                \
        return ret_val;                                                        \
    }
