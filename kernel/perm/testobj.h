/**
 * @file testobj.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 测试对象, 用于测试能力系统
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

namespace perm::testobj {
    // TestObject的权限定义
    // 该对象仅用于测试能力系统, 因此权限非常简单
    constexpr b64 READ     = 0x1'0000;
    constexpr b64 WRITE    = 0x2'0000;
    constexpr b64 INCREASE = 0x4'0000;
    constexpr b64 DECREASE = 0x8'0000;
}  // namespace perm::testobj