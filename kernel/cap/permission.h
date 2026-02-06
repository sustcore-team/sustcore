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
#include <sus/types.h>

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
        constexpr b64 ALLOC = 0x1'0000;
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
        // 则该能力被克隆出的能力中, 相应权限的SLOT_READ, SLOT_INSERT, SLOT_REMOVE, SLOT_SHARE位都将被清除
        // 用于防止槽位的二次分发
        constexpr b64 SLOT_SHARE  = 0x8;

        constexpr size_t slot_offset(size_t slot_idx) noexcept {
            return slot_idx * SLOT_BITS;
        }

        // 向权限位图中写入某个槽位的权限
        constexpr void set_slot(b64 *bitmap, size_t slot_idx, b64 permission) {
            size_t bit_pos      = slot_offset(slot_idx);
            size_t bitmap_index = bit_pos / 64;
            size_t bit_offset   = bit_pos % 64;
            // 清除原有权限位
            b64 aligned_perm = (permission & SLOT_MASK) << bit_offset;
            // clear the original bits
            b64 clear_mask   = ~(SLOT_MASK << bit_offset);
            bitmap[bitmap_index] &= clear_mask;
            // set new permission bits
            bitmap[bitmap_index] |= aligned_perm;
        }
    }  // namespace capspace
}  // namespace perm

struct PermissionBits {
    // 基础权限位, 绝大多数能力只需要这64个位
    // 我们规定, 低16位保留给基础能力使用, 剩余48位供派生能力使用
    b64 basic_permissions;
    // 扩展权限位图
    // 例如 FILE 能力, NOTIFICATION 能力,
    // CSPACE能力指向的内核对象实质上包含了一系列对象
    // 对这些对象进行更细粒度的权限控制时, 就需要使用权限位图
    b64 *permission_bitmap;

    enum class Type { NONE = 0, BASIC = 1, CAPSPACE = 2 };
    const Type type;

    constexpr static size_t to_bitmap_size(Type type) noexcept {
        switch (type) {
            case Type::NONE:  return 0;
            case Type::BASIC: return 0;
            case Type::CAPSPACE:
                return CAP_SPACE_SIZE / 64 * perm::cspace::SLOT_BITS;  // 每个b64可以表示64个位
            default: return 0;
        }
    }

    constexpr PermissionBits(b64 basic, b64 *bitmap, Type type)
        : basic_permissions(basic), permission_bitmap(bitmap), type(type) {}
    constexpr PermissionBits(b64 basic, Type type)
        : basic_permissions(basic), permission_bitmap(nullptr), type(type) {
        // assert (to_bitmap_size(type) == 0);
    }
    constexpr ~PermissionBits() {
        if (permission_bitmap != nullptr) {
            delete[] permission_bitmap;
            permission_bitmap = nullptr;
        }
    }

    constexpr PermissionBits(PermissionBits &&other) noexcept
        : permission_bitmap(other.permission_bitmap), type(other.type) {
        other.permission_bitmap = nullptr;
    }
    constexpr PermissionBits &operator=(PermissionBits &&other) noexcept {
        // assert (this.type == other.type);
        if (this != &other) {
            permission_bitmap       = other.permission_bitmap;
            other.permission_bitmap = nullptr;
        }
        return *this;
    }

    PermissionBits(const PermissionBits &other)            = delete;
    PermissionBits &operator=(const PermissionBits &other) = delete;

    constexpr bool imply(const PermissionBits &other) const noexcept {
        if (this->type != other.type) {
            return false;
        }
        // 首先比较 basic_permissions
        if (!BITS_IMPLIES(this->basic_permissions, other.basic_permissions)) {
            return false;
        }
        // 之后再比较 permission_bitmap
        size_t bitmap_size      = to_bitmap_size(this->type);
        bool permission_implied = true;
        /**
         * 为什么采用逐b64比较并将结果进行累加,
         * 而不是在判定到某一位不满足时直接返回false? 在这里, 我的考量是,
         * 权限位图并不会特别巨大(不超过4096字节), 且绝大多数情况下,
         * 权限校验都是成功的. 因此, 提前返回无法节省多少时间,
         * 甚至可能因为分支预测失败而带来额外的开销(虽然概率不大) 而且,
         * 我选择相信编译器.
         * 编译器极有可能会使用SIMD指令来优化这个循环(尽管RISC-V下好像没有,
         * 但似乎存在类似的向量指令集) 同时也能够避免分支预测失败带来的性能损失.
         * 综上所述, 我认为这种实现方式在大多数情况下性能更优.
         * 也许我是错的? 但我确实认为这种方法是效率更加的选择.
         * 也许需要做一个测试, 但还是等到后面说吧.
         * TODO: 进行性能测试, 比较提前返回与否的性能差异
         */
        for (size_t i = 0; i < bitmap_size; i++) {
            permission_implied &= BITS_IMPLIES(this->permission_bitmap[i],
                                               other.permission_bitmap[i]);
        }
        return permission_implied;
    }

    constexpr bool imply(b64 basic_permission) const noexcept {
        return BITS_IMPLIES(this->basic_permissions, basic_permission);
    }

    // 从第offset个位出发, 检查至多64个位的权限是否被包含
    // 注: 当 offset + 64 超过权限位图范围时, 只检查至多权限位图范围的部分
    constexpr bool imply(b64 permission_b64, size_t offset) const noexcept {
        if (this->type != Type::CAPSPACE) {
            return false;
        }
        size_t bit_pos      = offset;
        size_t bitmap_index = bit_pos / 64;
        size_t bit_offset   = bit_pos % 64;

        b64 relevant_bits = this->permission_bitmap[bitmap_index] >> bit_offset;
        // 如果 permission_b64 跨越了两个b64
        if (bit_offset + 64 > 64) {
            relevant_bits |=
                this->permission_bitmap[bitmap_index + 1]
                << (64 - bit_offset);
        }
        return BITS_IMPLIES(relevant_bits, permission_b64);
    }
};