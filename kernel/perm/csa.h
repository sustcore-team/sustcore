/**
 * @file csa_perm.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CSpace Accessor Permissions
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/types.h>
#include <sustcore/capability.h>

namespace perm::csa {
    // CSpace Accessor的权限定义
    // basic permissions
    constexpr b64 ALLOC   = 0x1'0000;
    constexpr b64 ACCEPT  = 0x2'0000;

    // extended permissions for slot control
    // CSpace Accessor中每个权限组控制一个CGroup中的所有槽位
    constexpr b64 SLOT_BITS   = 4;
    // 只有通过CSpaceAccessor能力访问slot时需要通过该权限位进行检验
    constexpr b64 SLOT_READ   = 0x1;
    // 向该槽位中插入能力对象(Create, Clone, Move等操作都需要该权限)
    constexpr b64 SLOT_INSERT = 0x2;
    // 向该槽位中移除能力对象
    constexpr b64 SLOT_REMOVE = 0x4;
    // 分享该槽位
    // 如果一个槽位的SLOT_SHARE权限为0
    // 则该能力被克隆出的能力中, 相应权限的SLOT_READ, SLOT_INSERT,
    // SLOT_REMOVE, SLOT_SHARE位都将被清除 用于防止槽位的二次分发
    constexpr b64 SLOT_SHARE  = 0x8;
    constexpr b64 BITMAP_SIZE =
        (CSPACE_SIZE * SLOT_BITS + 63) / 64;  // 向上取整为b64的数量

    // 得到位图偏移
    inline size_t bitmap_offset(size_t group_idx) {
        return group_idx * SLOT_BITS;
    }
}  // namespace perm::csa