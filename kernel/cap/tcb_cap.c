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

//============================================
// TCB能力权限常量
//============================================

const qword TCB_PRIV_SET_PRIORITY[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0001ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword TCB_PRIV_SUSPEND[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0002ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword TCB_PRIV_RESUME[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0004ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword TCB_PRIV_TERMINATE[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0008ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword TCB_PRIV_YIELD[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0010ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword TCB_PRIV_WAIT_NOTIFICATION[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0020ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

//============================================
// TCB能力构造相关操作
//============================================

CapPtr create_tcb_cap(PCB *p, TCB *tcb) {
    return create_cap(p, CAP_TYPE_TCB, (void *)tcb, CAP_ALL_PRIV, nullptr);
}

CapPtr tcb_cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p,
                      qword priv[PRIVILEDGE_QWORDS]) {
    TCB_CAP_START(src_p, src_ptr, tcb_cap_derive, cap, tcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);
    (void)tcb;

    // 进行派生
    return derive_cap(dst_p, cap, priv, nullptr);
}

CapPtr tcb_cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p) {
    TCB_CAP_START(src_p, src_ptr, tcb_cap_clone, cap, tcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);

    (void)tcb;  // 未使用, 特地标记以避免编译器警告

    // 进行完全克隆
    return tcb_cap_derive(src_p, src_ptr, dst_p, cap->cap_priv);
}

CapPtr tcb_cap_degrade(PCB *p, CapPtr cap_ptr,
                       qword cap_priv[PRIVILEDGE_QWORDS]) {
    TCB_CAP_START(p, cap_ptr, tcb_cap_degrade, cap, tcb, CAP_NONE_PRIV,
                  INVALID_CAP_PTR);
    (void)tcb;  // 未使用, 特地标记以避免编译器警告

    if (!degrade_cap(p, cap, cap_priv)) {
        return INVALID_CAP_PTR;
    }

    return cap_ptr;
}

TCB *tcb_cap_unpack(PCB *p, CapPtr cap_ptr) {
    TCB_CAP_START(p, cap_ptr, tcb_cap_unpack, cap, tcb, CAP_PRIV_UNPACK,
                  nullptr);

    return tcb;
}

//============================================
// TCB能力对象操作相关操作
//============================================

/**
 * @brief 将线程切换到yield状态
 *
 * @param p 当前进程的PCB
 * @param ptr 能力指针
 */
void tcb_cap_yield(PCB *p, CapPtr cap_ptr) {
    TCB_CAP_START(p, cap_ptr, tcb_cap_yield, cap, tcb, TCB_PRIV_YIELD, );

    // 切换线程状态到yield
    if (tcb->state != TS_RUNNING) {
        log_error("只能对运行中的线程进行yield操作! (tid=%d, state=%d)",
                  tcb->tid, tcb->state);
        return;
    }

    tcb->state = TS_YIELD;
}