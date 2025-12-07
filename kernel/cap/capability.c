/**
 * @file capability.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability系统
 * @version alpha-1.0.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <assert.h>
#include <cap/capability.h>
#include <mem/alloc.h>
#include <string.h>
#include <sus/list_helper.h>
#include <task/task_struct.h>

#ifdef DLOG_CAP
#define DISABLE_LOGGING
#endif

#include <basec/logger.h>

Capability *fetch_cap(PCB *pcb, CapPtr ptr) {
    log_info("fetch_cap: cspace=%d, cindex=%d", ptr.cspace, ptr.cindex);

    // PCB中无CSpaces
    if (pcb->cap_spaces == nullptr) {
        log_error("fetch_cap: PCB块中无CSpaces");
        return nullptr;
    }

    // 对应CSpace不存在
    if (ptr.cspace < 0 || ptr.cspace >= PROC_CSPACES) {
        log_error("fetch_cap: CSpace超出范围");
        return nullptr;
    }

    if (pcb->cap_spaces[ptr.cspace] == nullptr) {
        log_error("fetch_cap: 对应的CSpace不存在");
        return nullptr;
    }

    if (ptr.cindex < 0 || ptr.cindex >= CSPACE_ITEMS) {
        log_error("fetch_cap: CIndex超出范围");
        return nullptr;
    }

    CSpace *space   = &pcb->cap_spaces[ptr.cspace];
    Capability *cap = (*space)[ptr.cindex];
    if (cap == nullptr) {
        log_error("fetch_cap: CIndex对应的Capability不存在");
        return nullptr;
    }
    return cap;
}

CapPtr insert_cap(PCB *pcb, Capability *cap) {
    if (cap == nullptr) {
        log_error("insert_cap: cap不能为空!");
        return INVALID_CAP_PTR;
    }

    if (pcb == nullptr) {
        log_error("insert_cap: pcb不能为空!");
        return INVALID_CAP_PTR;
    }

    // PCB中无CSpaces
    if (pcb->cap_spaces == nullptr) {
        log_error("insert_cap: PCB块中无CSpaces");
        return INVALID_CAP_PTR;
    }

    // 遍历CSpaces
    for (int i = 0; i < PROC_CSPACES; i++) {
        if (pcb->cap_spaces[i] == nullptr) {
            // 创建新的CapSpace
            pcb->cap_spaces[i] =
                (Capability **)kmalloc(sizeof(Capability *) * CSPACE_ITEMS);
            memset(pcb->cap_spaces[i], 0, sizeof(Capability *) * CSPACE_ITEMS);
        }
        CSpace *space = &pcb->cap_spaces[i];
        for (int j = 0; j < CSPACE_ITEMS; j++) {
            // 跳过INVALID_CAP_PTR
            if (i + j == 0) {
                continue;
            }
            // 找到空位
            if ((*space)[j] == nullptr) {
                (*space)[j]  = cap;
                CapPtr ptr   = (CapPtr){.cspace = i, .cindex = j};
                cap->cap_ptr = ptr;
                // 插入链表
                list_push_back(cap, CAPABILITY_LIST(pcb));
                return ptr;
            }
        }
    }

    log_error("insert_cap: PCB中槽位已满!");
    return INVALID_CAP_PTR;
}

CapPtr create_cap(PCB *p, CapType type, void *cap_data, void *cap_priv) {
    if (p == nullptr) {
        log_error("create_cap: PCB不能为空!");
        return INVALID_CAP_PTR;
    }
    if (cap_data == nullptr) {
        log_error("create_cap: 能力数据为空!");
        return INVALID_CAP_PTR;
    }
    if (cap_priv == nullptr) {
        log_error("create_cap: 能力权限为空!");
        return INVALID_CAP_PTR;
    }
    // 构造能力对象
    Capability *cap = (Capability *)kmalloc(sizeof(Capability));
    memset(cap, 0, sizeof(Capability));
    // 初始化派生能力链表
    list_init(CHILDREN_CAP_LIST(cap));
    // 设置能力属性
    cap->type     = type;
    cap->pcb      = p;
    cap->cap_data = cap_data;
    cap->cap_priv = cap_priv;
    // 插入能力到PCB
    return insert_cap(p, cap);
}


/**
 * @brief 派生能力
 *
 * @param p 在p内派生能力
 * @param parent 父能力
 * @param cap_priv 子能力权限
 * @return CapPtr 能力指针
 */
CapPtr derive_cap(PCB *p, Capability *parent, void *cap_priv)
{
    if (parent == nullptr) {
        log_error("derive_cap: 父能力不能为空!");
        return INVALID_CAP_PTR;
    }
    if (cap_priv == nullptr) {
        log_error("derive_cap: 子能力权限不能为空!");
        return INVALID_CAP_PTR;
    }
    // 构造能力对象
    Capability *cap = (Capability *)kmalloc(sizeof(Capability));
    memset(cap, 0, sizeof(Capability));
    // 初始化派生能力链表
    list_init(CHILDREN_CAP_LIST(cap));
    // 设置能力属性
    cap->type     = parent->type;
    cap->cap_data = parent->cap_data;
    cap->cap_priv = cap_priv;
    cap->parent   = parent;
    // 插入能力到PCB
    CapPtr ptr = insert_cap(p, cap);
    if (ptr.val == INVALID_CAP_PTR.val) {
        return INVALID_CAP_PTR;
    }
    // 将新能力加入到父能力的派生链表中
    list_push_back(cap, CHILDREN_CAP_LIST(parent));
    return ptr;
}