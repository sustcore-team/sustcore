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
#include <string.h>
#include <mem/alloc.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif
#include <basec/logger.h>

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

    return create_cap(p, CAP_TYPE_NOT, (void *)notif, (void *)priv);
}

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
                      NotificationCapPriv priv)
{
    NOT_CAP_START(src_p, src_ptr, not_cap_derive, cap, notif, old_priv, INVALID_CAP_PTR);

    (void)notif; // 未使用, 特地标记以避免编译器警告

    if (!old_priv->priv_derive) {
        log_error("该能力不具有derive权限!");
        return INVALID_CAP_PTR;
    }

    // 检查新权限是否有效
    // 逐个QWORD检查
    // 新权限位图必须是旧权限位图的子集
    // 即 old_priv => priv
    for (int i = 0; i < NOTIFICATION_BITMAP_QWORDS; i++)
    {
        if (!BITS_IMPLIES(old_priv->priv_check[i], priv.priv_check[i]) ||
            !BITS_IMPLIES(old_priv->priv_set[i],   priv.priv_set[i])   ||
            !BITS_IMPLIES(old_priv->priv_reset[i], priv.priv_reset[i]))
        {
            log_error("派生的权限无效!");
            return INVALID_CAP_PTR;
        }
    }

    if (! BOOL_IMPLIES(old_priv->priv_derive, priv.priv_derive)) {
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
    NOT_CAP_START(src_p, src_ptr, not_cap_clone, cap, notif, old_priv, INVALID_CAP_PTR);

    (void)notif; // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return not_cap_derive(src_p, src_ptr, dst_p, *old_priv);
}

void not_cap_set(PCB *p, CapPtr ptr, int notification_id)
{
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
}

/**
 * @brief 清除通知位
 *
 * @param p 当前进程PCB指针
 * @param ptr 能力指针
 * @param notification_id 通知ID (0-255)
 */
void not_cap_reset(PCB *p, CapPtr ptr, int notification_id)
{
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
bool not_cap_check(PCB *p, CapPtr ptr, int notification_id)
{
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
