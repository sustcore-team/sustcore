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

const qword CAP_PRIV_UNPACK[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0001ull,
    [1] = 0x0000'0000'0000'0000ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword CAP_PRIV_DERIVE[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0002ull,
    [1] = 0x0000'0000'0000'0000ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

const qword CAP_ALL_PRIV[PRIVILEDGE_QWORDS] = {
    [0] = 0xFFFF'FFFF'FFFF'FFFFull,
    [1] = 0xFFFF'FFFF'FFFF'FFFFull,
    [2] = 0xFFFF'FFFF'FFFF'FFFFull,
    [3] = 0xFFFF'FFFF'FFFF'FFFFull,
};

const qword CAP_NONE_PRIV[PRIVILEDGE_QWORDS] = {
    [0] = 0x0000'0000'0000'0000ull,
    [1] = 0x0000'0000'0000'0000ull,
    [2] = 0x0000'0000'0000'0000ull,
    [3] = 0x0000'0000'0000'0000ull,
};

/**
 * @brief 创建一个新的CSpace
 *
 * @return CSpace* 新的CSpace指针
 */
CSpace *new_cspace(void) {
    CSpace *space = (CSpace *)kmalloc(sizeof(Capability *) * CSPACE_ITEMS);
    memset(space, 0, sizeof(Capability *) * CSPACE_ITEMS);
    return space;
}

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
            pcb->cap_spaces[i] = new_cspace();
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

/**
 * @brief 向pcb中插入cap, 插入到指定位置
 *
 * @param pcb 进程控制块
 * @param cap 能力
 * @param cap_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr insert_cap_at(PCB *pcb, Capability *cap, CapPtr cap_ptr) {
    if (cap == nullptr) {
        log_error("insert_cap_at: cap不能为空!");
        return INVALID_CAP_PTR;
    }

    if (pcb == nullptr) {
        log_error("insert_cap_at: pcb不能为空!");
        return INVALID_CAP_PTR;
    }

    if (pcb->cap_spaces == nullptr) {
        log_error("insert_cap_at: PCB块中无CSpaces");
        return INVALID_CAP_PTR;
    }

    // 检查cap_ptr是否合法
    if (cap_ptr.cspace < 0 || cap_ptr.cspace >= PROC_CSPACES) {
        log_error("insert_cap_at: CSpace超出范围");
        return INVALID_CAP_PTR;
    }

    if (cap_ptr.cindex < 0 || cap_ptr.cindex >= CSPACE_ITEMS) {
        log_error("insert_cap_at: CIndex超出范围");
        return INVALID_CAP_PTR;
    }

    // 若对应CSpace不存在, 则创建
    if (pcb->cap_spaces[cap_ptr.cspace] == nullptr) {
        pcb->cap_spaces[cap_ptr.cspace] = new_cspace();
    }

    CSpace *space = &pcb->cap_spaces[cap_ptr.cspace];
    // 检查指定位置是否已被占用
    if ((*space)[cap_ptr.cindex] != nullptr) {
        log_error("insert_cap_at: 指定位置已被占用");
        return INVALID_CAP_PTR;
    }

    // 插入能力
    (*space)[cap_ptr.cindex] = cap;
    cap->cap_ptr             = cap_ptr;
    // 插入链表
    list_push_back(cap, CAPABILITY_LIST(pcb));
    return cap_ptr;
}

/**
 * @brief 构造能力
 *
 * @param p 需要构造能力的进程控制块
 * @param type 能力类型
 * @param cap_data 能力数据
 * @param cap_priv 能力权限
 * @param attached_priv 附加权限
 * @return Capability* 能力指针
 */
static Capability *__create_cap(PCB *p, CapType type, void *cap_data,
                                const qword cap_priv[PRIVILEDGE_QWORDS],
                                void *attached_priv) {
    if (p == nullptr) {
        log_error("create_cap: PCB不能为空!");
        return nullptr;
    }
    if (cap_data == nullptr) {
        log_error("create_cap: 能力数据为空!");
        return nullptr;
    }
    if (cap_priv == nullptr) {
        log_error("create_cap: 能力权限为空!");
        return nullptr;
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
    memcpy(cap->cap_priv, cap_priv, sizeof(qword) * PRIVILEDGE_QWORDS);
    cap->attached_priv = attached_priv;
    return cap;
}

CapPtr create_cap(PCB *p, CapType type, void *cap_data,
                  const qword cap_priv[PRIVILEDGE_QWORDS],
                  void *attached_priv) {
    // 构造能力对象
    Capability *cap = __create_cap(p, type, cap_data, cap_priv, attached_priv);
    if (cap == nullptr) {
        return INVALID_CAP_PTR;
    }

    // 插入能力到PCB
    CapPtr ret = insert_cap(p, cap);
    if (ret.val == INVALID_CAP_PTR.val) {
        kfree(cap);
        return INVALID_CAP_PTR;
    }
    return ret;
}

CapPtr create_cap_at(PCB *p, CapType type, void *cap_data,
                     const qword cap_priv[PRIVILEDGE_QWORDS],
                     void *attached_priv, CapPtr cap_ptr) {
    // 构造能力对象
    Capability *cap = __create_cap(p, type, cap_data, cap_priv, attached_priv);
    if (cap == nullptr) {
        return INVALID_CAP_PTR;
    }

    // 插入能力到PCB
    CapPtr ret = insert_cap_at(p, cap, cap_ptr);
    if (ret.val == INVALID_CAP_PTR.val) {
        kfree(cap);
        return INVALID_CAP_PTR;
    }
    return ret;
}

/**
 * @brief 派生能力
 *
 * @param dst_p 目标进程控制块
 * @param parent 父能力
 * @param cap_priv 子能力权限
 * @param attached_priv 附加权限
 * @return Capability* 能力
 */
static Capability *__derive_cap(PCB *dst_p, Capability *parent,
                                qword cap_priv[PRIVILEDGE_QWORDS],
                                void *attached_priv) {
    // 进行权限检查
    if (!derivable(parent->cap_priv, cap_priv) ||
        !derivable(parent->cap_priv, CAP_PRIV_DERIVE))
    {
        log_error("__derive_cap: 父能力权限不包含子能力权限, 无法派生!");
        return nullptr;
    }

    // 构造能力对象
    Capability *cap = (Capability *)kmalloc(sizeof(Capability));
    memset(cap, 0, sizeof(Capability));

    // 初始化派生能力链表
    list_init(CHILDREN_CAP_LIST(cap));

    // 设置能力属性
    cap->type     = parent->type;
    cap->cap_data = parent->cap_data;

    memcpy(cap->cap_priv, cap_priv, sizeof(qword) * PRIVILEDGE_QWORDS);
    cap->attached_priv = attached_priv;

    return cap;
}

CapPtr derive_cap(PCB *p, Capability *parent, qword cap_priv[PRIVILEDGE_QWORDS],
                  void *attached_priv) {
    if (parent == nullptr) {
        log_error("derive_cap: 父能力不能为空!");
        return INVALID_CAP_PTR;
    }
    if (cap_priv == nullptr) {
        log_error("derive_cap: 子能力权限不能为空!");
        return INVALID_CAP_PTR;
    }

    // 派生能力对象
    Capability *cap = __derive_cap(p, parent, cap_priv, attached_priv);
    if (cap == nullptr) {
        return INVALID_CAP_PTR;
    }

    // 插入能力到PCB
    CapPtr ptr = insert_cap(p, cap);
    if (ptr.val == INVALID_CAP_PTR.val) {
        kfree(cap);
        return INVALID_CAP_PTR;
    }

    // 将新能力加入到父能力的派生链表中
    cap->parent = parent;
    list_push_back(cap, CHILDREN_CAP_LIST(parent));
    return ptr;
}

CapPtr derive_cap_at(PCB *p, Capability *parent,
                     qword cap_priv[PRIVILEDGE_QWORDS], void *attached_priv,
                     CapPtr cap_ptr) {
    if (parent == nullptr) {
        log_error("derive_cap_at: 父能力不能为空!");
        return INVALID_CAP_PTR;
    }
    if (cap_priv == nullptr) {
        log_error("derive_cap_at: 子能力权限不能为空!");
        return INVALID_CAP_PTR;
    }

    // 派生能力对象
    Capability *cap = __derive_cap(p, parent, cap_priv, attached_priv);
    if (cap == nullptr) {
        return INVALID_CAP_PTR;
    }

    // 插入能力到PCB
    CapPtr ptr = insert_cap_at(p, cap, cap_ptr);
    if (ptr.val == INVALID_CAP_PTR.val) {
        kfree(cap);
        return INVALID_CAP_PTR;
    }

    // 将新能力加入到父能力的派生链表中
    cap->parent = parent;
    list_push_back(cap, CHILDREN_CAP_LIST(parent));
    return ptr;
}

bool degrade_cap(PCB *p, Capability *cap,
                 const qword new_priv[PRIVILEDGE_QWORDS]) {
    // 附加权限由各个能力类型自行检查

    if (!derivable(cap->cap_priv, new_priv)) {
        log_error("degrade_cap: 父能力权限不包含子能力权限, 无法降级!");
        return false;
    }

    // 复制新能力权限
    memcpy(cap->cap_priv, new_priv, sizeof(qword) * PRIVILEDGE_QWORDS);
    return true;
}

const char *cap_type_to_string(CapType type) {
    switch (type) {
        case CAP_TYPE_NUL:    return "CAP_TYPE_NUL";
        case CAP_TYPE_PRC:    return "CAP_TYPE_PRC";
        case CAP_TYPE_PCB:    return "CAP_TYPE_PCB";
        case CAP_TYPE_THR:    return "CAP_TYPE_THR";
        case CAP_TYPE_TCB:    return "CAP_TYPE_TCB";
        case CAP_TYPE_DEV:    return "CAP_TYPE_DEV";
        case CAP_TYPE_FLE:    return "CAP_TYPE_FLE";
        case CAP_TYPE_INT:    return "CAP_TYPE_INT";
        case CAP_TYPE_MEM:    return "CAP_TYPE_MEM";
        case CAP_TYPE_PRO:    return "CAP_TYPE_PRO";
        case CAP_TYPE_CUSTOM: return "CAP_TYPE_CUSTOM";
        case CAP_TYPE_NOT:    return "CAP_TYPE_NOT";
        default:              return "Invalid type";
    }
}