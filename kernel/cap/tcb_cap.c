/**
 * @file tcb_cap.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief TCB能力相关操作
 * @version alpha-1.0.0
 * @date 2025-12-11
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <cap/capability.h>
#include <cap/tcb_cap.h>
#include <mem/alloc.h>
#include <string.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

CapPtr create_tcb_cap(PCB *p, TCB *tcb) {
    TCBCapPriv default_priv = {
        .priv_unwrap            = true,
        .priv_set_priority      = true,
        .priv_suspend           = true,
        .priv_resume            = true,
        .priv_terminate         = true,
        .priv_yield             = true,
        .priv_wait_notification = true,
    };

    TCBCapPriv *priv = (TCBCapPriv *)kmalloc(sizeof(TCBCapPriv));
    memcpy(priv, &default_priv, sizeof(TCBCapPriv));
    return create_cap(p, CAP_TYPE_TCB, (void *)tcb, (void *)priv);
}

TCB *tcb_cap_unwrap(PCB *p, CapPtr ptr) {
    TCB_CAP_START(p, ptr, tcb_cap_unwrap, cap, tcb, priv, nullptr);

    // 是否有对应权限
    if (!priv->priv_unwrap) {
        log_error("该能力不具有unwrap权限!");
        return nullptr;
    }

    return tcb;
}

/**
 * @brief 将线程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 */
void tcb_cap_yield(PCB *p, CapPtr ptr) {
    TCB_CAP_START(p, ptr, tcb_cap_yield, cap, tcb, priv, );

    // 是否有对应权限
    if (!priv->priv_yield) {
        log_error("该能力不具有yield权限!");
        return;
    }

    // 切换线程状态到yield
    if (tcb->state != TS_RUNNING) {
        log_error("只能对运行中的线程进行yield操作! (tid=%d, state=%d)",
                  tcb->tid, tcb->state);
        return;
    }

    tcb->state = TS_YIELD;
}