/**
 * @file pcb_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief PCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>
#include <task/task_struct.h>

// 退出进程权限
#define PCB_PRIV_EXIT          (0x0000'0000'0001'0000ull)
// fork新进程权限
#define PCB_PRIV_FORK          (0x0000'0000'0002'0000ull)
// 获取PID权限
#define PCB_PRIV_GETPID        (0x0000'0000'0004'0000ull)
// 创建线程权限
#define PCB_PRIV_CREATE_THREAD (0x0000'0000'0008'0000ull)
// 遍历能力权限
#define PCB_PRIV_ENUM_CAPS     (0x0000'0000'0010'0000ull)
// 迁移能力权限
#define PCB_PRIV_MIGRATE_CAPS  (0x0000'0000'0020'0000ull)

/**
 * @brief 构造PCB能力
 *
 * @param p 需要构造PCB能力的进程
 * @return CapPtr 能力指针
 */
CapPtr create_pcb_cap(PCB *p);

/**
 * @brief 从源进程的源能力派生一个新的PCB能力到目标进程
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr pcb_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p, qword priv);

/**
 * @brief 从源进程的源能力派生一个新的PCB能力到目标进程的指定位置
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 目标能力指针位置
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr pcb_cap_derive_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr,
                         qword priv);

/**
 * @brief 从源进程的源能力克隆一个PCB能力到目标进程
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr pcb_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p);

/**
 * @brief 从源进程的源能力克隆一个PCB能力到目标进程的指定位置
 *
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 目标能力指针位置
 * @return CapPtr 新的能力指针
 */
CapPtr pcb_cap_clone_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr);

/**
 * @brief 将能力降级为更低权限的能力
 *
 * @param p 当前进程PCB指针
 * @param cap_ptr 能力指针
 * @param cap_priv 新的能力权限
 * @return CapPtr 降级后的能力指针(和cap_ptr相同)
 */
CapPtr pcb_cap_degrade(PCB *p, CapPtr cap_ptr, qword cap_priv);

/**
 * @brief 解包PCB能力, 获得PCB指针
 *
 * @param p 当前进程PCB指针
 * @param cap_ptr 能力指针
 * @return PCB* PCB指针
 */
PCB *pcb_cap_unpack(PCB *p, CapPtr cap_ptr);

/**
 * @brief 退出进程
 *
 * @param p 当前进程的PCB
 * @param cap_ptr 能力指针
 */
void pcb_cap_exit(PCB *p, CapPtr cap_ptr);

/**
 * @brief fork一个新进程
 *
 * @param p 当前进程的PCB
 * @param cap_ptr 能力指针
 * @param child_cap 返回新进程的能力指针
 * @return PCB* 新进程的PCB指针
 */
PCB *pcb_cap_fork(PCB *p, CapPtr cap_ptr, CapPtr *child_cap);

/**
 * @brief 获取进程的PID
 *
 * @param p 当前进程的PCB
 * @param cap_ptr 能力指针
 * @return pid_t 进程的PID
 */
pid_t pcb_cap_getpid(PCB *p, CapPtr cap_ptr);

/**
 * @brief 在cap_ptr所指向的进程创建一个新线程
 *
 * @param p 当前进程的PCB
 * @param cap_ptr 能力指针
 * @param entrypoint 线程入口点
 * @param priority 线程优先级
 * @return CapPtr 新线程的能力指针
 */
CapPtr pcb_cap_create_thread(PCB *p, CapPtr cap_ptr, void *entrypoint,
                             int priority);

#define PCB_CAP_START(proc, cap_ptr, cap, pcb, priv_check, ret_val) \
    Capability *cap = fetch_cap(proc, cap_ptr);                     \
    if (cap == nullptr) {                                           \
        log_error("%s:指针指向的能力不存在!", __FUNCTION__);        \
        return ret_val;                                             \
    }                                                               \
    if (cap->type != CAP_TYPE_PCB) {                                \
        log_error("%s:该能力不为PCB能力!", __FUNCTION__);           \
        return ret_val;                                             \
    }                                                               \
    if (cap->cap_data == nullptr) {                                 \
        log_error("%s:能力数据为空!", __FUNCTION__);                \
        return ret_val;                                             \
    }                                                               \
    PCB *pcb = (PCB *)cap->cap_data;                                \
    if (!derivable(cap->cap_priv, priv_check)) {                    \
        log_error("%s:能力权限不足!", __FUNCTION__);                \
        return ret_val;                                             \
    }
