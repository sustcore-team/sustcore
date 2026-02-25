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

#include <perm/perm.h>
#include <sus/raii.h>
#include <sus/types.h>
#include <sustcore/capability.h>

#include <cassert>
#include <cstring>

template <typename T>
concept ValidPB = std::same_as<T, b8> || std::same_as<T, b16> ||
                  std::same_as<T, b32> || std::same_as<T, b64>;

struct PermissionBits {
    // 基础权限位, 绝大多数能力只需要这64个位
    // 我们规定, 低16位保留给基础能力使用, 剩余48位供派生能力使用
    b64 basic_permissions;
    // 扩展权限位图
    // 例如 FILE 能力, NOTIFICATION 能力,
    // CSPACE能力指向的内核对象实质上包含了一系列对象
    // 对这些对象进行更细粒度的权限控制时, 就需要使用权限位图
    b64 *permission_bitmap;

    const PayloadType type;

    constexpr static size_t to_bitmap_size(PayloadType type) noexcept {
        switch (type) {
            case PayloadType::NONE:     return 0;
            case PayloadType::TEST_OBJECT: return 0;
            case PayloadType::CSPACE_ACCESSOR: return perm::csa::BITMAP_SIZE;
            default:             return 0;
        }
    }

    PermissionBits(b64 basic, const b64 *bitmap, PayloadType type);
    // used by capability with no permission bitmap
    PermissionBits(b64 basic, PayloadType type);
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

    // 从第offset个位出发, 检查至多sz个位的权限是否被包含
    // 注: 当 offset + sz 超过权限位图范围时, 只检查至多权限位图范围的部分
    template <ValidPB _PB, size_t bitcnt = sizeof(_PB) * 8>
    bool implies(_PB permbits, size_t offset) const noexcept {
        static_assert(sizeof(_PB) * 8 >= bitcnt,
                      "bitcnt must not exceed the number of bits in _PB");
        const size_t bitpos      = offset;
        const size_t bitmap_idx  = bitpos / 64;
        const size_t bitoff      = bitpos % 64;
        const size_t bitmap_size = to_bitmap_size(this->type);

        if (bitmap_size == 0 || this->permission_bitmap == nullptr) {
            return false;
        }

        // offset 超过权限位图范围
        if (bitmap_idx >= bitmap_size) {
            return permbits == 0;
        }

        const b64 *bitmap = this->permission_bitmap;
        _PB relbits       = bitmap[bitmap_idx] >> bitoff;

        // 如果 permbits 跨越了两个b64
        if ((64 - bitoff < bitcnt) && (bitmap_idx + 1 < bitmap_size)) {
            size_t rem_bitcnt  = bitcnt - bitoff;
            const b64 rembits  = bitmap[bitmap_idx + 1] << rem_bitcnt;
            relbits           |= rembits;
        }

        return BITS_IMPLIES(relbits, permbits);
    }

    // 复制权限
    PermissionBits clone() const;
    static PermissionBits allperm(PayloadType type);
};