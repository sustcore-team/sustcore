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

enum class CapType {
    NONE      = 0,
    CAP_SPACE = 1,
};

union CapIdx {
    // Note: 我们不考虑大端序机器
    struct {
        // 低位
        b32 slot : 32;
        // 高位
        b32 space : 32;
    };
    b64 raw;

    constexpr CapIdx(b64 raw = 0) : raw(raw){};
    // 然而, 构造函数中, 我们先指明space再指明slot,
    // 以符合我们平时习惯的空间在前, 槽位在后的表达方式
    constexpr CapIdx(b32 space, b32 slot) : slot(slot), space(space){};

    constexpr bool nullable(void) const noexcept {
        return raw == 0;
    }
};

enum class CapErrCode {
    SUCCESS                  = 0,
    INVALID_CAPABILITY       = -1,
    INVALID_INDEX            = -2,
    INSUFFICIENT_PERMISSIONS = -3,
    TYPE_NOT_MATCHED         = -4,
    PAYLOAD_ERROR            = -5,
    CREATION_FAILED          = -6,
    UNKNOWN_ERROR            = -255,
};