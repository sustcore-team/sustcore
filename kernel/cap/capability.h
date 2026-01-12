/**
 * @file capability.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability系统
 * @version alpha-1.0.0
 * @date 2025-12-06
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <sus/capability.h>

struct PCBStruct;
typedef struct PCBStruct PCB;

#define MAX_PRIVILEDGES   (256)
#define PRIVILEDGE_QWORDS (MAX_PRIVILEDGES / (8 * sizeof(qword)))

/**
 * @brief 能力
 *
 */
typedef struct CapStruct {
    // 能力类型
    CapType type;
    /**
     * @brief 能力数据
     *
     * 指向该能力所对应的数据
     *
     */
    void *cap_data;
    /**
     * @brief 能力权限
     *
     * 指向该能力所对应的权限
     * 添加新权限时, 应当查看对应能力的create与derive函数
     * 确保新权限被正确地添加到权限结构体中, 并在派生函数中进行检查
     *
     */
    qword cap_priv[PRIVILEDGE_QWORDS];
    /**
     * @brief 附加权限
     *
     * 对于部分类型的能力, 其可能需要更加复杂的权限表示
     *
     */
    void *attached_priv;
    // 所归属的进程控制块
    PCB *pcb;
    // 在块中的指针
    CapPtr cap_ptr;
    // 派生出的能力, 形成链表结构
    struct CapStruct *children_head;
    struct CapStruct *children_tail;
    struct CapStruct *sprev;
    struct CapStruct *snext;
    // 父能力
    struct CapStruct *parent;
    // 进程持有的所有能力, 形成链表结构
    struct CapStruct *prev;
    struct CapStruct *next;
} Capability;

#define CHILDREN_CAP_LIST(cap) \
    cap->children_head, cap->children_tail, snext, sprev

#define CSPACE_ITEMS (256)

typedef Capability **CSpace;

#define PROC_CSPACES (512)

/**
 * @brief 从pcb中取出ptr指向的cap
 *
 * @param pcb 能力所附属的进程控制块
 * @param ptr 能力指针
 * @return Capability* 能力
 */
Capability *fetch_cap(PCB *pcb, CapPtr ptr);

/**
 * @brief 创建一个新的CSpace
 *
 * @return CSpace* 新的CSpace指针
 */
CSpace *new_cspace(void);

/**
 * @brief 向pcb中插入cap
 *
 * @param pcb 能力需要插入的进程控制块
 * @param cap 能力
 * @return CapPtr 能力指针
 */
CapPtr insert_cap(PCB *pcb, Capability *cap);

/**
 * @brief 向pcb的指定位置中插入cap
 *
 * @param pcb 能力需要插入的进程控制块
 * @param cap 能力
 * @param cap_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr insert_cap_at(PCB *pcb, Capability *cap, CapPtr cap_ptr);

/**
 * @brief 构造能力
 *
 * @param p 需要构造能力的进程控制块
 * @param type 能力类型
 * @param cap_data 能力数据
 * @param cap_priv 能力权限
 * @param attached_priv 附加权限
 * @return CapPtr 能力指针
 */
CapPtr create_cap(PCB *p, CapType type, void *cap_data,
                  const qword cap_priv[PRIVILEDGE_QWORDS], void *attached_priv);

/**
 * @brief 在指定位置构造能力
 *
 * @param p 需要构造能力的进程控制块
 * @param type 能力类型
 * @param cap_data 能力数据
 * @param cap_priv 能力权限
 * @param attached_priv 附加权限
 * @param cap_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr create_cap_at(PCB *p, CapType type, void *cap_data,
                     const qword cap_priv[PRIVILEDGE_QWORDS],
                     void *attached_priv, CapPtr cap_ptr);

/**
 * @brief 权限检查
 *
 * @param parent_priv 父权限
 * @param child_priv 子权限
 * @return true 父权限包含子权限
 * @return false 父权限不包含子权限
 */
inline static bool derivable(const qword parent_priv[PRIVILEDGE_QWORDS],
                             const qword child_priv[PRIVILEDGE_QWORDS]) {
    for (int i = 0; i < PRIVILEDGE_QWORDS; i++) {
        if ((parent_priv[i] & child_priv[i]) != child_priv[i]) {
            return false;
        }
    }
    return true;
}

// 解包能力权限
extern const qword CAP_PRIV_UNPACK[PRIVILEDGE_QWORDS];
// 派生能力权限
extern const qword CAP_PRIV_DERIVE[PRIVILEDGE_QWORDS];
// 全部权限
extern const qword CAP_ALL_PRIV[PRIVILEDGE_QWORDS];
// 无权限
extern const qword CAP_NONE_PRIV[PRIVILEDGE_QWORDS];

/**
 * @brief 派生能力
 *
 * @param p 在p内派生能力
 * @param parent 父能力
 * @param cap_priv 子能力权限
 * @param attached_priv 附加权限
 * @return CapPtr 能力指针
 */
CapPtr derive_cap(PCB *p, Capability *parent, qword cap_priv[PRIVILEDGE_QWORDS],
                  void *attached_priv);

/**
 * @brief 派生能力
 *
 * @param p 在p内派生能力
 * @param parent 父能力
 * @param cap_priv 子能力权限
 * @param attached_priv 附加权限
 * @param cap_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr derive_cap_at(PCB *p, Capability *parent,
                     qword cap_priv[PRIVILEDGE_QWORDS], void *attached_priv,
                     CapPtr cap_ptr);

/**
 * @brief 降级能力
 *
 * @param p 进程控制块
 * @param cap_ptr 能力指针
 * @param new_priv 新权限
 * @return true 降级成功
 * @return false 降级失败
 */
bool degrade_cap(PCB *p, Capability *cap,
                 const qword new_priv[PRIVILEDGE_QWORDS]);

/**
 * @brief 能力类型转字符串
 *
 * @param type 能力类型
 * @return const char* 字符串
 */
const char *cap_type_to_string(CapType type);