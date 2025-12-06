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
    // yield进程的能力
    bool priv_yield;
    // exit进程的能力
    bool priv_exit;
    // fork进程的能力
    bool priv_fork;
    // 获得pid的能力
    bool priv_getpid;
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
 * @return PCB* 新进程的PCB指针
 */
PCB *pcb_cap_fork(PCB *p, CapPtr ptr);

/**
 * @brief 获取进程的PID
 * 
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 * @return pid_t 进程的PID
 */
pid_t pcb_cap_getpid(PCB *p, CapPtr ptr);