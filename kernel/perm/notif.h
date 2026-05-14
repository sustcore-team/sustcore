/**
 * @file notif.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Notification Permissions
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

#include <cstddef>

namespace perm::notif {
    // 发送信号/取消信号的权限
    constexpr b8 SIGNAL   = 0b01;
    // 查询信号/等待信号的权限
    constexpr b8 QUERY    = 0b10;
    // 一个信号所需的权限位数
    constexpr size_t BITS = 2;
    constexpr size_t STARTBITS =
        16;  // 从第16位开始分配信号权限(即从0x0001'0000开始)
    constexpr size_t MASK = 0b11;  // 每个信号占用的权限位掩码
    // 总共支持的信号数量
    constexpr size_t MAX_SIGNALS = (sizeof(b64) * 8 - STARTBITS) / BITS;

    // 计算第idx个信号的权限位偏移
    constexpr size_t bitoffset(size_t idx) {
        return STARTBITS + idx * BITS;
    }

    // 获取第idx个信号的权限值
    constexpr b8 perm(size_t permbits, size_t idx) {
        return (permbits >> bitoffset(idx)) & MASK;
    }
}  // namespace perm::notif