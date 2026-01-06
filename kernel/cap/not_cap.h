/**
 * @file not_cap.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通知能力
 * @version alpha-1.0.0
 * @date 2025-12-23
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <cap/capability.h>

// Notification被定义为一个长256个位的位图
// 每个位代表一个不同的通知类型
// Notification能力也与TCB, PCB能力有所联动
// 具体来说, 当进程/线程具有挂起自身的能力, 且具有检查某个通知ID的能力时
// 进程/线程就可以将自身挂起, 直到该通知ID被设置
// 进一步地, 进程/线程可以等待一类通知ID的集合
// 只要集合中的任意一个通知ID被设置, 进程/线程就会被唤醒
// 这种设计允许进程/线程高效地等待多种事件的发生
// 而不需要为每个事件单独创建等待机制

// 通知能力数据
typedef struct {
    int notif_id;
    qword bitmap[NOTIFICATION_BITMAP_QWORDS];  // 通知位图
} Notification;

// 通知能力权限
typedef struct {
    qword priv_set[NOTIFICATION_BITMAP_QWORDS];  // 设置通知的权限位图
    qword priv_reset[NOTIFICATION_BITMAP_QWORDS];  // 清除通知的权限位
    qword priv_check[NOTIFICATION_BITMAP_QWORDS];  // 检查通知的权限位
    bool priv_derive;                              // 派生能力的权限
} NotificationCapPriv;

/**
 * @brief 构造Notification能力
 *
 * @param p 在p内构造一个Notification能力
 * @return  CapPtr 能力指针
 */
CapPtr create_notification_cap(PCB *p);

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
                      NotificationCapPriv priv);

/**
 * @brief 从src_p的src_ptr能力克隆一个Notification能力到dst_p
 *
 * @param src_p   源进程
 * @param src_ptr 源能力
 * @param dst_p   目标进程
 * @return CapPtr 派生出的能力指针
 */
CapPtr not_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p);

/**
 * @brief 设置通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @param notification_id 通知ID (0-255)
 */
void not_cap_set(PCB *p, CapPtr ptr, int notification_id);

/**
 * @brief 清除通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 通知能力指针
 * @param notification_id 通知ID (0-255)
 */
void not_cap_reset(PCB *p, CapPtr ptr, int notification_id);

/**
 * @brief 检查通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 通知能力指针
 * @param notification_id 通知ID (0-255)
 * @return true 通知已设置
 * @return false 通知未设置
 */
bool not_cap_check(PCB *p, CapPtr ptr, int notification_id);

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
                               qword *wait_bitmap);

#define NOT_CAP_START(src_p, src_ptr, fun, cap, notif, priv, ret) \
    Capability *cap = fetch_cap(src_p, src_ptr);                  \
    if (cap == nullptr) {                                         \
        log_error(#fun ":指针指向的能力不存在!");                 \
        return ret;                                               \
    }                                                             \
    if (cap->type != CAP_TYPE_NOT) {                              \
        log_error(#fun ":该能力不为Notification能力!");           \
        return ret;                                               \
    }                                                             \
    if (cap->cap_data == nullptr) {                               \
        log_error(#fun ":能力数据为空!");                         \
        return ret;                                               \
    }                                                             \
    if (cap->cap_priv == nullptr) {                               \
        log_error(#fun ":能力权限为空!");                         \
        return ret;                                               \
    }                                                             \
    Notification *notif       = (Notification *)cap->cap_data;    \
    NotificationCapPriv *priv = (NotificationCapPriv *)cap->cap_priv
