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

    constexpr static bool is_inline(PayloadType type) noexcept {
        return ! (type & PayloadType::NOINLINE_MASK);
    }

    constexpr static size_t to_bitmap_size(PayloadType type) noexcept {
        if (is_inline(type))
            return 0;
        switch (type) {
            case PayloadType::CSPACE_ACCESSOR: return perm::csa::BITMAP_SIZE;
            default:                           return 0;
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

    bool complete_imply(const PermissionBits &other) const noexcept;

    inline bool basic_imply(b64 basic_permission) const noexcept {
        return BITS_IMPLIES(this->basic_permissions, basic_permission);
    }

    // 从第offset个位出发, 检查至多sz个位的权限是否被包含
    // 不允许跨越b64的边界, 也就是说, offset与offset+sz必须在同一个b64内
    template <ValidPB _PB, size_t bitcnt = sizeof(_PB) * 8>
    bool implies(_PB permbits, size_t offset) const noexcept {
        assert(!is_inline(this->type));
        static_assert(sizeof(_PB) * 8 >= bitcnt,
                      "bitcnt must not exceed the number of bits in _PB");
        static_assert(bitcnt > 0, "bitcnt must be greater than 0");
        static_assert(bitcnt < 64, "bitcnt must not exceed 64");
        const size_t bitpos      = offset;
        const size_t bitmap_idx  = bitpos / 64;
        const size_t bitoff      = bitpos % 64;
        const size_t bitmap_size = to_bitmap_size(this->type);

        assert(bitmap_size > 0);

        if (this->permission_bitmap == nullptr) {
            return false;
        }

        // offset 超过权限位图范围
        assert(bitmap_idx < bitmap_size);
        assert(bitoff + bitcnt <= 64);
        const b64 *bitmap = this->permission_bitmap;
        return BITS_IMPLIES(bitmap[bitmap_idx] >> bitoff, permbits);
    }

    template<PayloadType T>
    bool quick_imply(const PermissionBits &other) const noexcept {
        assert(T == this->type);
        if (T != other.type) return false;
        if constexpr (is_inline(T)) {
            return basic_imply(other.basic_permissions);
        } else {
            return this->complete_imply(other);
        }
    }

    // 复制权限
    PermissionBits clone() const;
    static PermissionBits allperm(PayloadType type);

protected:
    struct AllPermTag {};
    PermissionBits(AllPermTag, PayloadType type);
};