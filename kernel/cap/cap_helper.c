/**
 * @file cap_helper.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力辅助函数
 * @version alpha-1.0.0
 * @date 2026-01-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cap/cap_helper.h>

CapPtr cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p, qword priv[PRIVILEDGE_QWORDS], void *attached_priv)
{
    Capability *src_cap = get_capability(src_p, src_ptr);
    if (src_cap == nullptr) {
        log_error("cap_derive: 源能力不存在");
        return INVALID_CAP_PTR;
    }

    CapPtr new_cap_ptr = INVALID_CAP_PTR;

    switch (src_cap->type) {
        case CAP_TYPE_PCB:
            new_cap_ptr = pcb_cap_derive(src_p, src_ptr, dst_p, priv);
            break;
        case CAP_TYPE_TCB:
            new_cap_ptr = tcb_cap_derive(src_p, src_ptr, dst_p, priv);
            break;
        case CAP_TYPE_NOT:
            new_cap_ptr = not_cap_derive(src_p, src_ptr, dst_p, priv,
                                         (NotCapPriv *)attached_priv);
            break;
        default:
            log_error("cap_derive: 不支持的能力类型");
            return INVALID_CAP_PTR;
    }

    return new_cap_ptr;
}

/**
 * @brief 导出一个能力到指定进程的指定位置
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr cap_derive_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr, qword priv[PRIVILEDGE_QWORDS], void *attached_priv)
{
    Capability *src_cap = get_capability(src_p, src_ptr);
    if (src_cap == nullptr) {
        log_error("cap_derive_at: 源能力不存在");
        return INVALID_CAP_PTR;
    }

    CapPtr new_cap_ptr = INVALID_CAP_PTR;

    switch (src_cap->type) {
        case CAP_TYPE_PCB:
            new_cap_ptr = pcb_cap_derive_at(src_p, src_ptr, dst_p, dst_ptr, priv);
            break;
        case CAP_TYPE_TCB:
            new_cap_ptr = tcb_cap_derive_at(src_p, src_ptr, dst_p, dst_ptr, priv);
            break;
        case CAP_TYPE_NOT:
            new_cap_ptr = not_cap_derive_at(src_p, src_ptr, dst_p, dst_ptr, priv,
                                            (NotCapPriv *)attached_priv);
            break;
        default:
            log_error("cap_derive_at: 不支持的能力类型");
            return INVALID_CAP_PTR;
    }

    return new_cap_ptr;
}

/**
 * @brief 克隆一个能力到指定进程
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @return CapPtr 能力指针
 */
CapPtr cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p)
{
    Capability *src_cap = get_capability(src_p, src_ptr);
    if (src_cap == nullptr) {
        log_error("cap_clone: 源能力不存在");
        return INVALID_CAP_PTR;
    }

    CapPtr new_cap_ptr = INVALID_CAP_PTR;

    switch (src_cap->type) {
        case CAP_TYPE_PCB:
            new_cap_ptr = pcb_cap_clone(src_p, src_ptr, dst_p);
            break;
        case CAP_TYPE_TCB:
            new_cap_ptr = tcb_cap_clone(src_p, src_ptr, dst_p);
            break;
        case CAP_TYPE_NOT:
            new_cap_ptr = not_cap_clone(src_p, src_ptr, dst_p);
            break;
        default:
            log_error("cap_clone: 不支持的能力类型");
            return INVALID_CAP_PTR;
    }

    return new_cap_ptr;
}

/**
 * @brief 克隆一个能力到指定进程的指定位置
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr cap_clone_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr)
{
    Capability *src_cap = get_capability(src_p, src_ptr);
    if (src_cap == nullptr) {
        log_error("cap_clone_at: 源能力不存在");
        return INVALID_CAP_PTR;
    }

    CapPtr new_cap_ptr = INVALID_CAP_PTR;

    switch (src_cap->type) {
        case CAP_TYPE_PCB:
            new_cap_ptr = pcb_cap_clone_at(src_p, src_ptr, dst_p, dst_ptr);
            break;
        case CAP_TYPE_TCB:
            new_cap_ptr = tcb_cap_clone_at(src_p, src_ptr, dst_p, dst_ptr);
            break;
        case CAP_TYPE_NOT:
            new_cap_ptr = not_cap_clone_at(src_p, src_ptr, dst_p, dst_ptr);
            break;
        default:
            log_error("cap_clone_at: 不支持的能力类型");
            return INVALID_CAP_PTR;
    }

    return new_cap_ptr;
}