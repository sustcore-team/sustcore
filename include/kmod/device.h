/**
 * @file device.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备管理接口
 * @version alpha-1.0.0
 * @date 2025-11-30
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stddef.h>
#include <kmod/capability.h>

/**
 * @brief 通过能力获取设备
 * 
 * 返回的是一个SHM能力, 指向设备的寄存器映射空间
 * 
 * @param cap 设备能力
 * @return Capability 设备的SHM能力
 */
Capability getdevice(Capability cap);

/**
 * @brief 注册中断服务程序
 * 
 * @param int_cap 中断能力
 * @param handler 中断处理函数
 */
void register_isr(Capability int_cap, void (*handler)(void));