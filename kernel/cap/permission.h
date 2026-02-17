/**
 * @file permission.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 权限
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capdef.h>
#include <string.h>
#include <sus/raii.h>
#include <sus/types.h>
#include <sustcore/cap_type.h>

#include <cassert>

namespace perm {
    namespace basic {
        // permissions
        constexpr b64 UNWRAP  = 0x0001;
        constexpr b64 CLONE   = 0x0002;
        constexpr b64 MIGRATE = 0x0004;
        // constexpr b64 DOWNGRADE = 0x0008;
        // constexpr b64 REMOVE    = 0x0010;
    }  // namespace basic
    namespace cspace {
        // permissions in basic bits
        // 从CSpace中寻找一个可用槽位
        // 对应方法寻找的槽位一定具有SLOT_INSERT权限
        // 且一定为空
        constexpr b64 ALLOC  = 0x1'0000;
        constexpr b64 ACCEPT = 0x2'0000;
        // 对每个槽位, 其权限控制存储在位图中

        // permission bits per slot
        // 每个槽位保留4个位用于权限控制
        constexpr b64 SLOT_BITS = 4;
        constexpr b64 SLOT_MASK = 0xF;

        // 只有通过CSpace的Capability接口访问slot时需要通过该权限位进行检验
        // 对于内核, 其可以直接通过lookup方法访问这些槽位
        constexpr b64 SLOT_READ   = 0x1;
        // 向该槽位中插入能力对象
        constexpr b64 SLOT_INSERT = 0x2;
        // 向该槽位中移除能力对象
        constexpr b64 SLOT_REMOVE = 0x4;
        // 分享该槽位
        // 如果一个槽位的SLOT_SHARE权限为0
        // 则该能力被克隆出的能力中, 相应权限的SLOT_READ, SLOT_INSERT,
        // SLOT_REMOVE, SLOT_SHARE位都将被清除 用于防止槽位的二次分发
        constexpr b64 SLOT_SHARE  = 0x8;

        constexpr size_t slot_offset(size_t slot_idx) noexcept {
            return slot_idx * SLOT_BITS;
        }

        // 向权限位图中写入某个槽位的权限
        constexpr void set_slot(b64 *bitmap, size_t slot_idx, b64 permission) {
            size_t bit_pos      = slot_offset(slot_idx);
            size_t bitmap_index = bit_pos / 64;
            size_t bit_offset   = bit_pos % 64;
            // TODO: 保证bit_offset + SLOT_BITS不超过64,
            // 否则需要跨越两个b64进行写入
            assert(bit_offset + SLOT_BITS <= 64);
            // 清除原有权限位
            b64 aligned_perm      = (permission & SLOT_MASK) << bit_offset;
            // clear the original bits
            b64 clear_mask        = ~(SLOT_MASK << bit_offset);
            bitmap[bitmap_index] &= clear_mask;
            // set new permission bits
            bitmap[bitmap_index] |= aligned_perm;
        }
    }  // namespace cspace
}  // namespace perm

struct PermissionBits {
    // 基础权限位, 绝大多数能力只需要这64个位
    // 我们规定, 低16位保留给基础能力使用, 剩余48位供派生能力使用
    b64 basic_permissions;
    // 扩展权限位图
    // 例如 FILE 能力, NOTIFICATION 能力,
    // CSPACE能力指向的内核对象实质上包含了一系列对象
    // 对这些对象进行更细粒度的权限控制时, 就需要使用权限位图
    util::Raii<b64[]> permission_bitmap;

    enum class Type : int { NONE = 0, BASIC = 1, CAPSPACE = 2 };
    const Type type;

    constexpr static size_t to_bitmap_size(Type type) noexcept {
        switch (type) {
            case Type::NONE:     return 0;
            case Type::BASIC:    return 0;
            case Type::CAPSPACE: {
                size_t total_bits = perm::cspace::SLOT_BITS * CSPACE_MAX_SLOTS;
                return (total_bits + 63) / 64;  // 向上取整为b64的数量
            }
            default: return 0;
        }
    }

    PermissionBits(b64 basic, const b64 *bitmap, Type type);
    // used by capability with no permission bitmap
    PermissionBits(b64 basic, Type type);
    ~PermissionBits();

    PermissionBits(PermissionBits &&other);
    PermissionBits(const PermissionBits &other) = delete;

    // permission bits不允许被赋值
    PermissionBits &operator=(PermissionBits &&)      = delete;
    PermissionBits &operator=(const PermissionBits &) = delete;

    // 但是我们提供一个downgrade方法, 允许在不改变权限类型的前提下,
    // 将权限位进行降级
    CapErrCode downgrade(const PermissionBits &new_perm);

    bool imply(const PermissionBits &other) const noexcept;

    inline bool imply(b64 basic_permission) const noexcept {
        return BITS_IMPLIES(this->basic_permissions, basic_permission);
    }

    // 从第offset个位出发, 检查至多64个位的权限是否被包含
    // 注: 当 offset + 64 超过权限位图范围时, 只检查至多权限位图范围的部分
    bool imply(b64 permission_b64, size_t offset) const noexcept;

    // 复制权限
    PermissionBits clone() const;
};