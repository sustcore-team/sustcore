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
#include <sustcore/errcode.h>

#include <expected>

// 载荷类型
enum class PayloadType : b64 {
    NONE            = 0,
    INLINE_PERM     = 0x0000,
    BITMAP_PERM     = 0x1000,
    NOINLINE_MASK   = 0x1000,
    // objects
    CSPACE_ACCESSOR = BITMAP_PERM | 0x001,
    INTOBJ          = INLINE_PERM | 0x002,
    SINTOBJ         = INLINE_PERM | 0x003,
    VFILE           = INLINE_PERM | 0x004
};

constexpr bool operator&(PayloadType a, PayloadType b) {
    return (static_cast<b64>(a) & static_cast<b64>(b)) != 0;
}

constexpr const char *to_string(PayloadType type) {
    switch (type) {
        case PayloadType::NONE:            return "NONE";
        case PayloadType::CSPACE_ACCESSOR: return "CSPACE_ACCESSOR";
        case PayloadType::INTOBJ:          return "INTOBJ";
        case PayloadType::SINTOBJ:         return "SINTOBJ";
        case PayloadType::VFILE:           return "VFILE";
        default:                           return "UNKNOWN";
    }
}

// CSpace中的CGroup数量
constexpr size_t CSPACE_SIZE    = 1024;
// SendSpace中的CGroup数量
constexpr size_t SENDSPACE_SIZE = 32;

// CGroup中的槽位数
// 每个槽位都存放着一个Capability
constexpr size_t CGROUP_SLOTS = 64;

// 每个CSpace中都有若干个CGroup, 每个CGroup又有若干个Capability
// 因此CSpace的容量为:
constexpr size_t CSPACE_CAPACITY = CSPACE_SIZE * CGROUP_SLOTS;

using CapIdx  = b64;
using RecvIdx = b64;

namespace capidx {
    template <b64 mask>
    consteval b64 CALC_MASK_SHIFT() {
        static_assert(mask != 0, "Mask cannot be zero");
        b64 shift = 0;
        b64 m     = mask;
        while ((m & 1) == 0) {
            m >>= 1;
            ++shift;
        }
        return shift;
    }

    constexpr CapIdx null  = 0;
    constexpr CapIdx error = 0xFFFFFFFFFFFFFFFF;

    constexpr b64 MASK_VALID  = 0x8000000000000000;
    constexpr b64 MASK_SLOT   = 0x0000000000FFFFFF;
    constexpr b64 SLOT_SHIFT  = CALC_MASK_SHIFT<MASK_SLOT>();
    constexpr b64 MASK_GROUP  = 0x0000FFFFFF000000;
    constexpr b64 GROUP_SHIFT = CALC_MASK_SHIFT<MASK_GROUP>();
    constexpr b64 MASK_RSVD =
        0xFFFFFFFFFFFFFFFF & ~(MASK_VALID | MASK_SLOT | MASK_GROUP);

    constexpr bool valid(CapIdx idx) {
        return (idx & MASK_VALID) != 0 && (idx & MASK_RSVD) == 0;
    }

    constexpr CapIdx make(b64 group, b64 slot) {
        return MASK_VALID | ((group << GROUP_SHIFT) & MASK_GROUP) |
               (slot & MASK_SLOT);
    }

    constexpr b64 group(CapIdx idx) {
        return (idx & MASK_GROUP) >> GROUP_SHIFT;
    }

    constexpr b64 slot(CapIdx idx) {
        return (idx & MASK_SLOT) >> SLOT_SHIFT;
    }
}  // namespace capidx

static_assert(sizeof(CapIdx) == sizeof(b64), "CapIdx must be 64 bits in size");