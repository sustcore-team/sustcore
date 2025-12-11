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

// PCB能力
typedef struct {
    // 获得PCB指针的能力
    bool priv_unwrap;
    // 派生能力的能力
    bool priv_derive;
    // yield进程的能力
    bool priv_yield;
    // exit进程的能力
    bool priv_exit;
    // fork进程的能力
    bool priv_fork;
    // 获得pid的能力
    bool priv_getpid;
    // 创建线程的能力
    bool priv_create_thread;
} PCBCapPriv;

/**
 * @brief 构造PCB能力
 *
 * @param p    在p内构造一个PCB能力
 * @param pcb  构造的能力指向pcb
 * @param priv 控制权限
 * @return CapPtr 能力指针
 */
CapPtr create_pcb_cap(PCB *p, PCB *pcb, PCBCapPriv priv);

/**
 * @brief 解包PCB能力, 获得PCB指针
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @return PCB* PCB指针
 */
PCB *pcb_cap_unwrap(PCB *p, CapPtr ptr);

/**
 * @brief 将进程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 */
void pcb_cap_yield(PCB *p, CapPtr ptr);

/**
 * @brief 退出进程
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 */
void pcb_cap_exit(PCB *p, CapPtr ptr);

/**
 * @brief fork一个新进程
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 * @param child_cap 返回新进程的能力指针
 * @return PCB* 新进程的PCB指针
 */
PCB *pcb_cap_fork(PCB *p, CapPtr ptr, CapPtr *child_cap);

/**
 * @brief 获取进程的PID
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 * @return pid_t 进程的PID
 */
pid_t pcb_cap_getpid(PCB *p, CapPtr ptr);

/**
 * @brief 在ptr所指向的进程创建一个新线程
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 * @param entrypoint 线程入口点
 * @param priority 线程优先级
 * @return CapPtr 新线程的能力指针
 */
CapPtr pcb_cap_create_thread(PCB *p, CapPtr ptr, void *entrypoint,
                             int priority);

/**
 * @brief 从src_p的src_ptr能力派生一个新的PCB能力到dst_p
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr pcb_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p, PCBCapPriv priv);

/**
 * @brief 从src_p的src_ptr能力派生一个新的PCB能力到dst_p, 并保留原权限
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param priv 新权限
 * @return CapPtr 新的能力指针
 */
CapPtr pcb_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p);