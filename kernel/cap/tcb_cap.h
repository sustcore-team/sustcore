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
} TCBCapPriv;

/**
 * @brief 构造TCB能力
 *
 * @param p    在p内构造一个TCB能力
 * @param tcb  构造的能力指向tcb
 * @param priv 控制权限
 * @return CapPtr 能力指针
 */
CapPtr create_tcb_cap(PCB *p, TCB *tcb, TCBCapPriv priv);

/**
 * @brief 解包TCB能力, 获得TCB指针
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @return TCB* TCB指针
 */
TCB *tcb_cap_unwrap(PCB *p, CapPtr ptr);