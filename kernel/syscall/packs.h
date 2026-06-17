/**
 * @file packs.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 参数包
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>

namespace syscall {
    struct ArgPack {
        b64 syscall_number;
        constexpr static size_t ARGS_SIZE = 7;
        b64 args[ARGS_SIZE];
    };

    struct RetPack {
        bool processed;
        b64 ret0;
        b64 ret1;
    };
}  // namespace syscall
