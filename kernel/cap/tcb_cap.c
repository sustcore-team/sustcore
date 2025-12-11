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

CapPtr create_tcb_cap(PCB *p, TCB *tcb, TCBCapPriv priv) {
    TCBCapPriv *priv_ptr = (TCBCapPriv *)kmalloc(sizeof(TCBCapPriv));
    memcpy(priv_ptr, &priv, sizeof(TCBCapPriv));
    return create_cap(p, CAP_TYPE_TCB, (void *)tcb, (void *)priv_ptr);
}

#define TCB_CAP_START(p, ptr, fun, cap, tcb, priv, ret) \
    Capability *cap = fetch_cap(p, ptr);                \
    if (cap == nullptr) {                               \
        log_error(#fun ":指针指向的能力不存在!");       \
        return ret;                                     \
    }                                                   \
    if (cap->type != CAP_TYPE_TCB) {                    \
        log_error(#fun ":该能力不为TCB能力!");          \
        return ret;                                     \
    }                                                   \
    if (cap->cap_data == nullptr) {                     \
        log_error(#fun ":能力数据为空!");               \
        return ret;                                     \
    }                                                   \
    if (cap->cap_priv == nullptr) {                     \
        log_error(#fun ":能力权限为空!");               \
        return ret;                                     \
    }                                                   \
    TCB *tcb         = (TCB *)cap->cap_data;            \
    TCBCapPriv *priv = (TCBCapPriv *)cap->cap_priv

TCB *tcb_cap_unwrap(PCB *p, CapPtr ptr) {
    TCB_CAP_START(p, ptr, tcb_cap_unwrap, cap, tcb, priv, nullptr);

    // 是否有对应权限
    if (!priv->priv_unwrap) {
        log_error("该能力不具有unwrap权限!");
        return nullptr;
    }

    return tcb;
}