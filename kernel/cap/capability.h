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
     *
     */
    void *cap_priv;
    // 所归属的进程控制块
    PCB *pcb;
    // 在块中的指针
    CapPtr cap_ptr;
    // 派生出的能力, 形成链表结构
    struct CapStruct *children_head;
    struct CapStruct *children_tail;
    struct CapStruct *sprev;
    struct CapStruct *snext;
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
 * @param pcb 进程控制块
 * @param ptr 能力指针
 * @return Capability* 能力
 */
Capability *fetch_cap(PCB *pcb, CapPtr ptr);

/**
 * @brief 向pcb中插入cap
 *
 * @param pcb 进程控制块
 * @param cap 能力
 * @return CapPtr 能力指针
 */
CapPtr insert_cap(PCB *pcb, Capability *cap);

/**
 * @brief 构造能力
 *
 * @param p 在p内构造能力
 * @param type 能力类型
 * @param cap_data 能力数据
 * @param cap_priv 能力权限
 * @return CapPtr 能力指针
 */
CapPtr create_cap(PCB *p, CapType type, void *cap_data, void *cap_priv);