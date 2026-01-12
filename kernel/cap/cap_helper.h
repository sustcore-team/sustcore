/**
 * @file cap_helper.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力辅助函数
 * @version alpha-1.0.0
 * @date 2026-01-11
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/capability.h>
#include <cap/capability.h>
#include <cap/not_cap.h>
#include <cap/pcb_cap.h>
#include <cap/tcb_cap.h>

/**
 * @brief 导出一个能力到指定进程
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @return CapPtr 能力指针
 */
CapPtr cap_derive(PCB *src_p, CapPtr src_ptr, PCB *dst_p, qword priv[PRIVILEDGE_QWORDS], void *attached_priv);

/**
 * @brief 导出一个能力到指定进程的指定位置
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr cap_derive_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr, qword priv[PRIVILEDGE_QWORDS], void *attached_priv);

/**
 * @brief 克隆一个能力到指定进程
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @return CapPtr 能力指针
 */
CapPtr cap_clone(PCB *src_p, CapPtr src_ptr, PCB *dst_p);

/**
 * @brief 克隆一个能力到指定进程的指定位置
 * 
 * @param src_p 源进程
 * @param src_ptr 源能力
 * @param dst_p 目标进程
 * @param dst_ptr 指定位置的能力指针
 * @return CapPtr 能力指针
 */
CapPtr cap_clone_at(PCB *src_p, CapPtr src_ptr, PCB *dst_p, CapPtr dst_ptr);