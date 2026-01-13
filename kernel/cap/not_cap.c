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

//============================================

// NOTIFICATION附加权限常量

const NotCapPriv NOTIFICATION_ALL_PRIV = {
    .priv_check = {QWORD_MAX, QWORD_MAX, QWORD_MAX, QWORD_MAX},
    .priv_set   = {QWORD_MAX, QWORD_MAX, QWORD_MAX, QWORD_MAX},
    .priv_reset = {QWORD_MAX, QWORD_MAX, QWORD_MAX, QWORD_MAX},
};

const NotCapPriv NOTIFICATION_NONE_PRIV = {
    .priv_check = {0, 0, 0, 0},
    .priv_set   = {0, 0, 0, 0},
    .priv_reset = {0, 0, 0, 0},
};

// NID计算辅助函数

static int nid_qword_idx(int nid) {
    return nid / 64;
}

static int nid_bit_idx(int nid) {
    return nid % 64;
}

// 权限操作函数

NotCapPriv *not_priv_set(NotCapPriv *priv, int nid) {
    int qword_index = nid_qword_idx(nid);
    int bit_index   = nid_bit_idx(nid);

    if (0 > qword_index || qword_index >= NOTIFICATION_BITMAP_QWORDS) {
        log_error("通知ID超出范围!");
        return priv;
    }

    if (bit_index < 0 || bit_index >= 64) {
        log_error("通知ID超出范围!");
        return priv;
    }

    priv->priv_set[qword_index] |= (1UL << bit_index);
    return priv;
}

NotCapPriv *not_priv_reset(NotCapPriv *priv, int nid) {
    int qword_index = nid_qword_idx(nid);
    int bit_index   = nid_bit_idx(nid);

    if (0 > qword_index || qword_index >= NOTIFICATION_BITMAP_QWORDS) {
        log_error("通知ID超出范围!");
        return priv;
    }

    if (bit_index < 0 || bit_index >= 64) {
        log_error("通知ID超出范围!");
        return priv;
    }

    priv->priv_reset[qword_index] |= (1UL << bit_index);
    return priv;
}

NotCapPriv *not_priv_check(NotCapPriv *priv, int nid) {
    int qword_index = nid_qword_idx(nid);
    int bit_index   = nid_bit_idx(nid);

    if (0 > qword_index || qword_index >= NOTIFICATION_BITMAP_QWORDS) {
        log_error("通知ID超出范围!");
        return priv;
    }

    if (bit_index < 0 || bit_index >= 64) {
        log_error("通知ID超出范围!");
        return priv;
    }

    priv->priv_check[qword_index] |= (1UL << bit_index);
    return priv;
}

bool notification_derivable(const NotCapPriv *parent_priv,
                            const NotCapPriv *child_priv) {
    for (int i = 0; i < NOTIFICATION_BITMAP_QWORDS; i++) {
        if ((parent_priv->priv_set[i] & child_priv->priv_set[i]) !=
            child_priv->priv_set[i])
        {
            return false;
        }
        if ((parent_priv->priv_reset[i] & child_priv->priv_reset[i]) !=
            child_priv->priv_reset[i])
        {
            return false;
        }
        if ((parent_priv->priv_check[i] & child_priv->priv_check[i]) !=
            child_priv->priv_check[i])
        {
            return false;
        }
    }
    return true;
}

//============================================
// Notification能力构造相关操作
//============================================

// NID分配计数器
static int NID_COUNTER = 0;

CapPtr create_notification_cap(PCB *p) {
    // 构造权限
    NotCapPriv *priv = (NotCapPriv *)kmalloc(sizeof(NotCapPriv));
    memcpy(priv, &NOTIFICATION_ALL_PRIV, sizeof(NotCapPriv));

    // 构造通知
    Notification *notif = (Notification *)kmalloc(sizeof(Notification));
    memset(notif, 0, sizeof(Notification));
    notif->notif_id = NID_COUNTER;
    NID_COUNTER++;

    CapPtr ptr = create_cap(p, CAP_TYPE_NOT, (void *)notif, CAP_PRIV_ALL,
                      (void *)priv);
    if (CAPPTR_INVALID(ptr)) {
        // 创建失败, 释放内存
        kfree(priv);
        kfree(notif);
        return INVALID_CAP_PTR;
    }
    return ptr;
}

CapPtr not_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p, qword cap_priv,
                      NotCapPriv *notif_priv) {
    NOT_CAP_START(src_p, src_ptr, cap, notif, CAP_PRIV_NONE,
                  &NOTIFICATION_NONE_PRIV, INVALID_CAP_PTR);
    (void)notif;  // 未使用, 特地标记以避免编译器警告

    // Capability权限由derive_cap检查
    // 此处检查通知权限
    // 并申请一块内存来拷贝权限数据
    if (!notification_derivable((NotCapPriv *)cap->attached_priv, notif_priv)) {
        log_error("not_cap_derive: 父能力权限不包含子能力权限, 无法派生!");
        return INVALID_CAP_PTR;
    }

    NotCapPriv *new_notif_priv = (NotCapPriv *)kmalloc(sizeof(NotCapPriv));
    memcpy(new_notif_priv, notif_priv, sizeof(NotCapPriv));

    // 进行派生
    CapPtr ptr = derive_cap(dst_p, cap, cap_priv, (void *)new_notif_priv);
    if (CAPPTR_INVALID(ptr)) {
        // 派生失败, 释放内存
        kfree(new_notif_priv);
        return INVALID_CAP_PTR;
    }

    return ptr;
}

CapPtr not_cap_derive_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr,
                         qword cap_priv, NotCapPriv *notif_priv) {
    NOT_CAP_START(src_p, src_ptr, cap, notif, CAP_PRIV_NONE,
                  &NOTIFICATION_NONE_PRIV, INVALID_CAP_PTR);
    (void)notif;  // 未使用, 特地标记以避免编译器警告

    // Capability权限由derive_cap检查
    // 此处检查通知权限
    // 并申请一块内存来拷贝权限数据
    if (!notification_derivable((NotCapPriv *)cap->attached_priv, notif_priv)) {
        log_error("not_cap_derive_at: 父能力权限不包含子能力权限, 无法派生!");
        return INVALID_CAP_PTR;
    }

    NotCapPriv *new_notif_priv = (NotCapPriv *)kmalloc(sizeof(NotCapPriv));
    memcpy(new_notif_priv, notif_priv, sizeof(NotCapPriv));

    // 进行派生
    CapPtr ptr =
        derive_cap_at(dst_p, cap, cap_priv, (void *)new_notif_priv, dst_ptr);
    if (CAPPTR_INVALID(ptr)) {
        // 派生失败, 释放内存
        kfree(new_notif_priv);
        return INVALID_CAP_PTR;
    }

    return ptr;
}

CapPtr not_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p) {
    NOT_CAP_START(src_p, src_ptr, cap, notif, CAP_PRIV_NONE,
                  &NOTIFICATION_NONE_PRIV, INVALID_CAP_PTR);
    (void)notif;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return not_cap_derive(src_p, src_ptr, dst_p, cap->cap_priv,
                          (NotCapPriv *)cap->attached_priv);
}

CapPtr not_cap_clone_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p,
                        CapPtr dst_ptr) {
    NOT_CAP_START(src_p, src_ptr, cap, notif, CAP_PRIV_NONE,
                  &NOTIFICATION_NONE_PRIV, INVALID_CAP_PTR);
    (void)notif;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return not_cap_derive_at(src_p, src_ptr, dst_p, dst_ptr, cap->cap_priv,
                             (NotCapPriv *)cap->attached_priv);
}

CapPtr not_cap_degrade(PCB *p, CapPtr cap_ptr, qword cap_priv,
                       NotCapPriv *notif_priv) {
    NOT_CAP_START(p, cap_ptr, cap, notif, CAP_PRIV_NONE,
                  &NOTIFICATION_NONE_PRIV, INVALID_CAP_PTR);
    (void)notif;  // 未使用, 特地标记以避免编译器警告

    // 只需要检查附加权限即可
    if (!notification_derivable((NotCapPriv *)cap->attached_priv, notif_priv)) {
        log_error("not_cap_degrade: 父能力权限不包含子能力权限, 无法降级!");
        return INVALID_CAP_PTR;
    }

    // 基础权限降级成功再进行附加权限更新
    if (!degrade_cap(p, cap, cap_priv)) {
        return INVALID_CAP_PTR;
    }

    memcpy(cap->attached_priv, notif_priv, sizeof(NotCapPriv));
    return cap_ptr;
}

Notification *not_cap_unpack(PCB *p, CapPtr cap_ptr) {
    NOT_CAP_START(p, cap_ptr, cap, notif, CAP_PRIV_UNPACK,
                  &NOTIFICATION_NONE_PRIV, nullptr);
    return notif;
}

//============================================
// Notification能力对象操作相关操作
//============================================

/**
 * @brief 检查通知是否已被设置
 *
 * @param notif 通知结构体指针
 * @param wait_bitmap 等待位图
 * @return true 通知已设置
 * @return false 通知未设置
 */
static bool is_notified(Notification *notif, qword *wait_bitmap) {
    // 检查等待位图与通知位图的交集
    for (int i = 0; i < NOTIFICATION_BITMAP_QWORDS; i++) {
        if (notif->bitmap[i] & wait_bitmap[i]) {
            return true;
        }
    }
    return false;
}

static void check_notification(Notification *notif) {
    // 遍历等待队列, 检查是否有线程/进程在等待该通知
    WaitingTCB *wtcb;
    foreach_list(wtcb, WAITING_LIST) {
        // 检查是否在等待该通知ID
        if (wtcb->reason.type != WAITING_NOTIFICATION ||
            wtcb->reason.notif.notif_id != notif->notif_id)
        {
            continue;
        }

        if (is_notified(notif, wtcb->reason.notif.bitmap)) {
            // 唤醒该线程/进程
            wakeup_thread(wtcb);
        }
    }
}

void not_cap_set(PCB *p, CapPtr cap_ptr, int nid) {
    // 计算需要的权限
    NotCapPriv required_priv = {
        .priv_check = {0, 0, 0, 0},
        .priv_set   = {0, 0, 0, 0},
        .priv_reset = {0, 0, 0, 0},
    };
    not_priv_set(&required_priv, nid);

    // 处理能力指针并校验能力
    NOT_CAP_START(p, cap_ptr, cap, notif, CAP_PRIV_NONE, &required_priv, );

    // 设置通知位
    notif->bitmap[nid_qword_idx(nid)] |= (1UL << nid_bit_idx(nid));

    // 检查是否有线程/进程在等待该通知, 并唤醒它们
    check_notification(notif);
}

/**
 * @brief 清除通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @param nid 通知ID (0-255)
 */
void not_cap_reset(PCB *p, CapPtr cap_ptr, int nid) {
    // 计算需要的权限
    NotCapPriv required_priv = {
        .priv_check = {0, 0, 0, 0},
        .priv_set   = {0, 0, 0, 0},
        .priv_reset = {0, 0, 0, 0},
    };
    not_priv_reset(&required_priv, nid);

    // 处理能力指针并校验能力
    NOT_CAP_START(p, cap_ptr, cap, notif, CAP_PRIV_NONE, &required_priv, );

    // 设置通知位
    notif->bitmap[nid_qword_idx(nid)] &= ~(1UL << nid_bit_idx(nid));
}

/**
 * @brief 检查通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @param nid 通知ID (0-255)
 * @return true 通知已设置
 * @return false 通知未设置
 */
bool not_cap_check(PCB *p, CapPtr cap_ptr, int nid) {
    // 计算需要的权限
    NotCapPriv required_priv = {
        .priv_check = {0, 0, 0, 0},
        .priv_set   = {0, 0, 0, 0},
        .priv_reset = {0, 0, 0, 0},
    };
    not_priv_check(&required_priv, nid);

    // 处理能力指针并校验能力
    NOT_CAP_START(p, cap_ptr, cap, notif, CAP_PRIV_NONE, &required_priv, false);

    // 检查通知位
    return (notif->bitmap[nid_qword_idx(nid)] & (1UL << nid_bit_idx(nid))) != 0;
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
    // 计算需要的权限
    NotCapPriv required_priv = {
        .priv_check = {0, 0, 0, 0},
        .priv_set   = {0, 0, 0, 0},
        .priv_reset = {0, 0, 0, 0},
    };
    memcpy(&required_priv.priv_check, wait_bitmap,
           sizeof(qword) * NOTIFICATION_BITMAP_QWORDS);
    NOT_CAP_START(p, not_ptr, cap, notif, CAP_PRIV_NONE, &required_priv, false);

    TCB_CAP_START(p, tcb_ptr, tcb_cap, tcb, TCB_PRIV_WAIT_NOTIFICATION, false);

    if (is_notified(notif, wait_bitmap)) {
        return true;
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