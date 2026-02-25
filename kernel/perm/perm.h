/**
 * @file perm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 权限定义
 * @version alpha-1.0.0
 * @date 2026-02-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <perm/csa.h>

namespace perm::basic {
    // permissions
    constexpr b64 UNWRAP  = 0x0001;
    constexpr b64 CLONE   = 0x0002;
    constexpr b64 MIGRATE = 0x0004;
}  // namespace perm::csa