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

/**
 * @brief 设置一个页表
 *
 * @return void* 页表指针
 */
void *setup_paging_table(void);

/**
 * @brief 为指定虚拟地址分配页
 *
 * @param root  页表根指针
 * @param vaddr 虚拟地址
 * @param pages 页数
 * @param rwx   权限
 * @param user  是否用户态
 */
void alloc_pages_for(void *root, void *vaddr, size_t pages, int rwx, bool user);

/**
 * @brief 从用户空间复制内存到用户空间
 *
 * @param dest_root 目标页表根指针
 * @param dest_vaddr 目标虚拟地址
 * @param src_root 源页表根指针
 * @param src_vaddr 源虚拟地址
 * @param size 大小
 */
void memcpy_u2u(void *dest_root, void *dest_vaddr, void *src_root, void *src_vaddr, size_t size);

/**
 * @brief 从内核空间复制内存到用户空间
 *
 * @param root 用户空间页表根指针
 * @param dest_vaddr 目标虚拟地址
 * @param src_kaddr 源内核地址
 * @param size 大小
 */
void memcpy_k2u(void *root, void *dest_vaddr, void *src_kaddr, size_t size);

/**
 * @brief 从用户空间复制内存到内核空间
 *
 * @param root 用户空间页表根指针
 * @param dest_kaddr 目标内核地址
 * @param src_vaddr 源用户虚拟地址
 * @param size 大小
 */
void memcpy_u2k(void *root, void *dest_kaddr, void *src_vaddr, size_t size);

/**
 * @brief 用户空间memset
 * 
 * @param root 用户空间页表根指针
 * @param vaddr 目标虚拟地址
 * @param byte 要设置的字节值
 * @param size 大小
 */
void memset_u(void *root, void *vaddr, int byte, size_t size);

/**
 * @brief 用户空间内存比较
 * 
 * @param root1 页表根指针1
 * @param vaddr1 虚拟地址1
 * @param root2 页表根指针2
 * @param vaddr2 虚拟地址2
 * @param size 大小
 * @return int 比较结果
 */
int memcmp_u2u(void *root1, void *vaddr1, void *root2, void *vaddr2, size_t size);