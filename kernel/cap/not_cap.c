/**
 * @file not_cap_.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通知能力
 * @version alpha-1.0.0
 * @date 2025-12-23
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <cap/not_cap.h>
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

static int NID_COUNTER = 0;

CapPtr create_notification_cap(PCB *p) {
    static const NotificationCapPriv NOTIFICATION_ALL_PRIV = {
        .priv_check  = {QWORD_MAX, QWORD_MAX, QWORD_MAX, QWORD_MAX},
        .priv_set    = {QWORD_MAX, QWORD_MAX, QWORD_MAX, QWORD_MAX},
        .priv_reset  = {QWORD_MAX, QWORD_MAX, QWORD_MAX, QWORD_MAX},
        .priv_derive = true};

    // 构造权限
    NotificationCapPriv *priv =
        (NotificationCapPriv *)kmalloc(sizeof(NotificationCapPriv));
    memcpy(priv, &NOTIFICATION_ALL_PRIV, sizeof(NotificationCapPriv));

    // 构造通知
    Notification *notif = (Notification *)kmalloc(sizeof(Notification));
    memset(notif, 0, sizeof(Notification));
    notif->notif_id = NID_COUNTER;
    NID_COUNTER++;

    return create_cap(p, CAP_TYPE_NOT, (void *)notif, (void *)priv);
}

/**
 * @brief 从src_p的src_ptr能力派生一个新的Notification能力到dst_p
 *
 * @param src_p   源进程
 * @param src_ptr 源能力
 * @param dst_p   目标进程
 * @param priv    新权限
 * @return CapPtr 派生出的能力指针
 */
CapPtr not_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p,
                      NotificationCapPriv priv) {
    NOT_CAP_START(src_p, src_ptr, not_cap_derive, cap, notif, old_priv,
                  INVALID_CAP_PTR);

    (void)notif;  // 未使用, 特地标记以避免编译器警告

    if (!old_priv->priv_derive) {
        log_error("该能力不具有derive权限!");
        return INVALID_CAP_PTR;
    }

    // 检查新权限是否有效
    // 逐个QWORD检查
    // 新权限位图必须是旧权限位图的子集
    // 即 old_priv => priv
    for (int i = 0; i < NOTIFICATION_BITMAP_QWORDS; i++) {
        if (!BITS_IMPLIES(old_priv->priv_check[i], priv.priv_check[i]) ||
            !BITS_IMPLIES(old_priv->priv_set[i], priv.priv_set[i]) ||
            !BITS_IMPLIES(old_priv->priv_reset[i], priv.priv_reset[i]))
        {
            log_error("派生的权限无效!");
            return INVALID_CAP_PTR;
        }
    }

    if (!BOOL_IMPLIES(old_priv->priv_derive, priv.priv_derive)) {
        log_error("派生的权限无效!");
        return INVALID_CAP_PTR;
    }

    // 进行派生
    NotificationCapPriv *new_priv =
        (NotificationCapPriv *)kmalloc(sizeof(NotificationCapPriv));
    memcpy(new_priv, &priv, sizeof(NotificationCapPriv));
    return derive_cap(dst_p, cap, (void *)new_priv);
}

/**
 * @brief 从src_p的src_ptr能力克隆一个Notification能力到dst_p
 *
 * @param src_p   源进程
 * @param src_ptr 源能力
 * @param dst_p   目标进程
 * @return CapPtr 派生出的能力指针
 */
CapPtr not_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p) {
    NOT_CAP_START(src_p, src_ptr, not_cap_clone, cap, notif, old_priv,
                  INVALID_CAP_PTR);

    (void)notif;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return not_cap_derive(src_p, src_ptr, dst_p, *old_priv);
}

void check_notification(Notification *notif) {
    // 遍历等待队列, 检查是否有线程/进程在等待该通知
    WaitingTCB *wtcb;
    foreach_list(wtcb, WAITING_LIST) {
        // 检查是否在等待该通知ID
        if (wtcb->reason.type != WAITING_NOTIFICATION ||
            wtcb->reason.notif.notif_id != notif->notif_id)
        {
            continue;
        }

        // 检查等待位图与通知位图的交集
        bool notified = false;
        for (int i = 0; i < NOTIFICATION_BITMAP_QWORDS; i++) {
            if (notif->bitmap[i] & wtcb->reason.notif.bitmap[i]) {
                notified = true;
                break;
            }
        }

        if (notified) {
            // 唤醒该线程/进程
            wakeup_thread(wtcb);
        }
    }
}

void not_cap_set(PCB *p, CapPtr ptr, int notification_id) {
    NOT_CAP_START(p, ptr, not_cap_set, cap, notif, priv, );

    int qword_index = notification_id / 64;
    int bit_index   = notification_id % 64;

    if (0 > qword_index || qword_index >= NOTIFICATION_BITMAP_QWORDS) {
        log_error("通知ID超出范围!");
        return;
    }

    if (bit_index < 0 || bit_index >= 64) {
        log_error("通知ID超出范围!");
        return;
    }

    // 检查权限
    if ((priv->priv_set[qword_index] & (1UL << bit_index)) == 0) {
        log_error("该能力不具有设置该通知ID的权限!");
        return;
    }

    // 设置通知位
    notif->bitmap[qword_index] |= (1UL << bit_index);

    // 检查是否有线程/进程在等待该通知, 并唤醒它们
    check_notification(notif);
}

/**
 * @brief 清除通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @param notification_id 通知ID (0-255)
 */
void not_cap_reset(PCB *p, CapPtr ptr, int notification_id) {
    NOT_CAP_START(p, ptr, not_cap_reset, cap, notif, priv, );

    int qword_index = notification_id / 64;
    int bit_index   = notification_id % 64;

    if (0 > qword_index || qword_index >= NOTIFICATION_BITMAP_QWORDS) {
        log_error("通知ID超出范围!");
        return;
    }

    if (bit_index < 0 || bit_index >= 64) {
        log_error("通知ID超出范围!");
        return;
    }

    // 检查权限
    if ((priv->priv_reset[qword_index] & (1UL << bit_index)) == 0) {
        log_error("该能力不具有清除该通知ID的权限!");
        return;
    }

    // 清除通知位
    notif->bitmap[qword_index] &= ~(1UL << bit_index);
}

/**
 * @brief 检查通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @param notification_id 通知ID (0-255)
 * @return true 通知已设置
 * @return false 通知未设置
 */
bool not_cap_check(PCB *p, CapPtr ptr, int notification_id) {
    NOT_CAP_START(p, ptr, not_cap_reset, cap, notif, priv, false);

    int qword_index = notification_id / 64;
    int bit_index   = notification_id % 64;

    if (0 > qword_index || qword_index >= NOTIFICATION_BITMAP_QWORDS) {
        log_error("通知ID超出范围!");
        return false;
    }

    if (bit_index < 0 || bit_index >= 64) {
        log_error("通知ID超出范围!");
        return false;
    }

    // 检查权限
    if ((priv->priv_reset[qword_index] & (1UL << bit_index)) == 0) {
        log_error("该能力不具有检查该通知ID的权限!");
        return false;
    }

    // 检查通知位
    return (notif->bitmap[qword_index] & (1UL << bit_index)) != 0;
}

/**
 * @brief 等待通知
 *
 * @param p 当前进程PCB指针
 * @param tcb_ptr TCB能力指针
 * @param not_ptr 通知能力指针
 * @param wait_bitmap 等待位图(应当总长256位)
 * @return true 通知已到达
 * @return false 通知未到达
 */
bool tcb_cap_wait_notification(PCB *p, CapPtr tcb_ptr, CapPtr not_ptr,
                               qword *wait_bitmap) {
    NOT_CAP_START(p, not_ptr, pcb_cap_wait_notification, not_cap, notif,
                  not_priv, false);
    TCB_CAP_START(p, tcb_ptr, pcb_cap_wait_notification, tcb_cap, tcb, tcb_priv,
                  false);

    if (tcb_priv->priv_wait_notification == false) {
        log_error("该PCB能力不具有等待通知的权限!");
        return false;
    }

    // 遍历位图确认是否可以等待
    for (int i = 0; i < NOTIFICATION_BITMAP_QWORDS; i++) {
        // 检查是否每个位都有权限等待
        if (wait_bitmap[i] & ~(not_priv->priv_check[i])) {
            log_error("等待位图中包含无权限等待的通知ID!");
            return false;
        }
    }

    // 向等待队列中添加当前线程
    // TODO: 这部分应当移交到scheduler模块中处理
    WaitingTCB *wtcb            = (WaitingTCB *)kmalloc(sizeof(WaitingTCB));
    wtcb->tcb                   = tcb;
    wtcb->reason.type           = WAITING_NOTIFICATION;
    wtcb->reason.notif.notif_id = notif->notif_id;
    wtcb->reason.notif.cap_ptr  = not_ptr;
    memcpy(wtcb->reason.notif.bitmap, wait_bitmap,
           sizeof(qword) * NOTIFICATION_BITMAP_QWORDS);
    tcb->state = TS_WAITING;
    list_push_front(wtcb, WAITING_LIST);

    // 检查是否有通知已经到达
    check_notification(notif);

    return true;
}