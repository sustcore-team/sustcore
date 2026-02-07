/**
 * @file permission.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief permissions
 * @version alpha-1.0.0
 * @date 2026-02-08
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/permission.h>
#include <kio.h>

PermissionBits::PermissionBits(b64 basic, b64 *bitmap, Type type)
    : basic_permissions(basic), permission_bitmap(nullptr), type(type) {
    size_t bitmap_size = to_bitmap_size(type);
    if (bitmap_size > 0) {
        permission_bitmap = new b64[bitmap_size];
        if (bitmap != nullptr)
            memcpy(permission_bitmap, bitmap, bitmap_size * sizeof(b64));
        else
            memset(permission_bitmap, 0, bitmap_size * sizeof(b64));
    } else if (bitmap != nullptr) {
        CAPABILITY::WARN(
            "具有类型%u的权限对象不应提供权限位图, 但实际提供了非空指针, "
            "已忽略该指针",
            static_cast<int>(type));
    }
}
PermissionBits::PermissionBits(b64 basic, Type type)
    : basic_permissions(basic), permission_bitmap(nullptr), type(type) {
    assert(to_bitmap_size(type) == 0);
}
PermissionBits::~PermissionBits() {
    if (permission_bitmap != nullptr) {
        delete[] permission_bitmap;
        permission_bitmap = nullptr;
    }
}

PermissionBits::PermissionBits(PermissionBits &&other)
    : basic_permissions(other.basic_permissions),
      permission_bitmap(other.permission_bitmap),
      type(other.type) {
    other.permission_bitmap = nullptr;
}

PermissionBits::PermissionBits(const PermissionBits &other)
    : PermissionBits(other.basic_permissions, other.permission_bitmap,
                     other.type) {}

bool PermissionBits::imply(const PermissionBits &other) const noexcept {
    if (this->type != other.type) {
        return false;
    }
    // 首先比较 basic_permissions
    if (!BITS_IMPLIES(this->basic_permissions, other.basic_permissions)) {
        return false;
    }
    // 之后再比较 permission_bitmap
    size_t bitmap_size = to_bitmap_size(this->type);
    if (bitmap_size == 0) {
        return true;
    }
    if (this->permission_bitmap == nullptr ||
        other.permission_bitmap == nullptr)
    {
        // 如果类型需要权限位图, 但其中一个权限对象的位图为nullptr,
        // 则视为不满足权限要求
        return false;
    }
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

// 从第offset个位出发, 检查至多64个位的权限是否被包含
// 注: 当 offset + 64 超过权限位图范围时, 只检查至多权限位图范围的部分
bool PermissionBits::imply(b64 permission_b64, size_t offset) const noexcept {
    if (this->type != Type::CAPSPACE) {
        return false;
    }
    size_t bit_pos      = offset;
    size_t bitmap_index = bit_pos / 64;
    size_t bit_offset   = bit_pos % 64;
    size_t bitmap_size  = to_bitmap_size(this->type);

    if (bitmap_size == 0 || this->permission_bitmap == nullptr) {
        return false;
    }

    // offset 超过权限位图范围
    if (bitmap_index >= bitmap_size) {
        return permission_b64 == 0;
    }

    b64 relevant_bits = this->permission_bitmap[bitmap_index] >> bit_offset;
    // 如果 permission_b64 跨越了两个b64
    if ((bit_offset != 0) && (bitmap_index + 1 < bitmap_size)) {
        relevant_bits |= this->permission_bitmap[bitmap_index + 1]
                         << (64 - bit_offset);
    }
    return BITS_IMPLIES(relevant_bits, permission_b64);
}

CapErrCode PermissionBits::downgrade(const PermissionBits &new_perm) {
    if (this->type != new_perm.type) {
        return CapErrCode::TYPE_NOT_MATCHED;
    }
    if (!imply(new_perm)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    this->basic_permissions = new_perm.basic_permissions;
    // imply方法会帮我们保证this->permission_bitmap与new_perm.permission_bitmap非空
    // however, 我们依然在这个地方放个断言
    assert(this->permission_bitmap != nullptr);
    assert(new_perm.permission_bitmap != nullptr);
    memcpy(this->permission_bitmap, new_perm.permission_bitmap,
           to_bitmap_size(this->type) * sizeof(b64));
    return CapErrCode::SUCCESS;
}