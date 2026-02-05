/**
 * @file cap_type.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力类型
 * @version alpha-1.0.0
 * @date 2026-02-05
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <sus/types.h>

enum class CapType { NONE = 0, BASIC = 1 };

union CapIdx {
    struct {
        b32 slot : 32;
        b32 space : 32;
    };
    b64 raw;
};