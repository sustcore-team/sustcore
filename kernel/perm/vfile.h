/**
 * @file vfile.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Virtual File Permissions
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

namespace perm::vfile {
    // VFile的权限定义
    // basic permissions
    constexpr b64 READ  = 0x01'0000;
    constexpr b64 WRITE = 0x02'0000;
    constexpr b64 EXEC  = 0x04'0000;
    constexpr b64 SEEK  = 0x10'0000;
    constexpr b64 SIZE  = 0x20'0000;
    constexpr b64 FLUSH = 0x40'0000;
}  // namespace perm::vfile