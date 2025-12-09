/**
 * @file vmm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 虚拟内存管理
 * @version alpha-1.0.0
 * @date 2025-12-03
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#include <stddef.h>

typedef enum {
    VMAT_NONE      = 0,
    VMAT_CODE      = 1,
    VMAT_DATA      = 2,
    VMAT_STACK     = 3,
    VMAT_HEAP      = 4,
    VMAT_MMAP      = 5,
    VMAT_SHARE_RW  = 6,
    VMAT_SHARE_RO  = 7,
    VMAT_SHARE_RX  = 8,
    VMAT_SHARE_RWX = 9
} VMAType;

typedef struct VMAStruct {
    struct VMAStruct *prev;
    struct VMAStruct *next;

    void *vaddr;
    size_t size;
    VMAType type;
} VMA;

typedef struct {
    // VMA链表
    VMA *vma_list_head;
    VMA *vma_list_tail;

    // 页表根地址
    void *pgd;
} TM;

// 有序链表
// 依据vaddr升序排列
#define TM_VMA_LIST(tm) \
    (tm)->vma_list_head, (tm)->vma_list_tail, prev, next, vaddr, ascending

/**
 * @brief 初始化进程内存空间
 *
 * @return TM 进程内存信息结构体
 */
TM *setup_task_memory(void);

/**
 * @brief 添加VMA
 *
 * @param tm 进程内存信息
 * @param vaddr VMA起始虚拟地址
 * @param size 大小
 * @param type VMA类型
 */
void add_vma(TM *tm, void *vaddr, size_t size, VMAType type);

/**
 * @brief 移除VMA
 *
 * @param tm 进程内存信息
 * @param vaddr VMA起始虚拟地址
 */
void remove_vma(TM *tm, void *vaddr);

/**
 * @brief 由虚拟地址查找对应VMA
 *
 * 查找vaddr所在的VMA
 *
 * @param tm 进程内存信息
 * @param vaddr 虚拟地址
 * @return VMA* 对应VMA指针
 * @return nullptr 未找到对应VMA
 */
VMA *find_vma(TM *tm, void *vaddr);

/**
 * @brief 从VMA类型转换为rwx权限
 *
 * @param type VMA类型
 * @return int rwx权限
 */
int from_seg_to_rwx(VMAType type);

/**
 * @brief 为指定虚拟地址分配页
 *
 * @param tm 进程内存信息
 * @param vaddr 虚拟地址
 * @param pages 页数
 * @param rwx 权限
 * @param user 是否用户态
 * @return true 分配成功
 * @return false 分配失败
 */
bool alloc_pages_for(TM *tm, void *vaddr, size_t pages, int rwx, bool user);

/**
 * @brief 页缺失处理
 *
 * @param tm 进程内存信息
 * @param vaddr 发生缺页的虚拟地址
 * @return true 处理成功
 * @return false 处理失败, 应当终止进程
 */
bool on_np_page_fault(TM *tm, void *vaddr);

/**
 * @brief 写入ro页错误处理
 *
 * @param tm 进程内存信息
 * @param vaddr 发生错误的虚拟地址
 * @return true 处理成功
 * @return false 处理失败, 应当终止进程
 */
bool on_write_ro_page_fault(TM *tm, void *vaddr);

/**
 * @brief 写入rx页错误处理
 *
 * @param tm 进程内存信息
 * @param vaddr 发生错误的虚拟地址
 * @return true 处理成功
 * @return false 处理失败, 应当终止进程
 */
bool on_write_rx_page_fault(TM *tm, void *vaddr);

/**
 * @brief 执行rw页错误处理
 *
 * @param tm 进程内存信息
 * @param vaddr 发生错误的虚拟地址
 * @return true 处理成功
 * @return false 处理失败, 应当终止进程
 */
bool on_execute_rw_page_fault(TM *tm, void *vaddr);

/**
 * @brief 从用户空间复制内存到用户空间
 *
 * @param dst_tm 目标进程内存信息
 * @param dst_vaddr 目标虚拟地址
 * @param src_tm 源进程内存信息
 * @param src_vaddr 源虚拟地址
 * @param size 大小
 */
void memcpy_u2u(TM *dst_tm, void *dst_vaddr, TM *src_tm,
                void *src_vaddr, size_t size);

/**
 * @brief 从内核空间复制内存到用户空间
 *
 * @param tm 进程内存信息
 * @param dst_vaddr 目标虚拟地址
 * @param src_kaddr 源内核地址
 * @param size 大小
 */
void memcpy_k2u(TM *tm, void *dst_vaddr, void *src_kaddr, size_t size);

/**
 * @brief 从用户空间复制内存到内核空间
 *
 * @param tm 进程内存信息
 * @param dst_kaddr 目标内核地址
 * @param src_vaddr 源用户虚拟地址
 * @param size 大小
 */
void memcpy_u2k(TM *tm, void *dst_kaddr, void *src_vaddr, size_t size);

/**
 * @brief 用户空间memset
 *
 * @param tm 进程内存信息
 * @param vaddr 目标虚拟地址
 * @param byte 要设置的字节值
 * @param size 大小
 */
void memset_u(TM *tm, void *vaddr, int byte, size_t size);

/**
 * @brief 用户空间内存比较
 *
 * @param tm1 进程内存信息1
 * @param vaddr1 虚拟地址1
 * @param tm2 进程内存信息2
 * @param vaddr2 虚拟地址2
 * @param size 大小
 * @return int 比较结果
 */
int memcmp_u2u(TM *tm1, void *vaddr1, TM *tm2, void *vaddr2,
               size_t size);