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

#include <kio.h>
#include <perm/permission.h>
#include <sus/defer.h>
#include <sus/raii.h>
#include <cstddef>

PermissionBits::PermissionBits(b64 basic, const b64 *bitmap, PayloadType type)
    : basic_permissions(basic),
      permission_bitmap(nullptr),
      type(type) {
    const size_t bitmap_size = to_bitmap_size(type);
    if (bitmap_size > 0) {
        permission_bitmap = new b64[bitmap_size];
        if (bitmap != nullptr) {
            memcpy(permission_bitmap, bitmap, bitmap_size * sizeof(b64));
        } else {
            memset(permission_bitmap, 0, bitmap_size * sizeof(b64));
        }
    } else if (bitmap != nullptr) {
        CAPABILITY::WARN(
            "具有类型%s的权限对象不应提供权限位图, 但实际提供了非空指针, "
            "已忽略该指针",
            to_string(type));
    }
}
PermissionBits::PermissionBits(b64 basic, PayloadType type)
    : basic_permissions(basic), permission_bitmap(nullptr), type(type) {
    assert(to_bitmap_size(type) == 0);
}
PermissionBits::~PermissionBits() {
    if (permission_bitmap != nullptr) {
        delete[] permission_bitmap;
    }
}

PermissionBits::PermissionBits(PermissionBits &&other)
    : basic_permissions(other.basic_permissions),
      permission_bitmap(other.permission_bitmap),
      type(other.type) {
    other.basic_permissions = 0;
    other.permission_bitmap = nullptr;
}

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
    if (this->permission_bitmap == nullptr || other.permission_bitmap == nullptr) {
        // 如果类型需要权限位图, 但其中一个权限对象的位图为nullptr,
        // 则视为不满足权限要求
        return false;
    }
    bool permission_implied = true;
    const b64 *this_bitmap  = this->permission_bitmap;
    const b64 *other_bitmap = other.permission_bitmap;
    /**
     * @brief TODO: 使用向量化方法重写该函数以提升性能
     *
     */
    for (size_t i = 0; i < bitmap_size; i++) {
        permission_implied &= BITS_IMPLIES(this_bitmap[i], other_bitmap[i]);
    }
    return permission_implied;
}

CapErrCode PermissionBits::downgrade(const PermissionBits &new_perm) {
    if (this->type != new_perm.type) {
        return CapErrCode::TYPE_NOT_MATCHED;
    }
    if (!imply(new_perm)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    this->basic_permissions = new_perm.basic_permissions;
    const size_t bitmap_size = to_bitmap_size(this->type);
    if (bitmap_size == 0) {
        return CapErrCode::SUCCESS;
    }
    // imply方法会帮我们保证this->permission_bitmap与new_perm.permission_bitmap非空
    // however, 我们依然在这个地方放个断言
    assert(this->permission_bitmap != nullptr);
    assert(new_perm.permission_bitmap != nullptr);
    memcpy(this->permission_bitmap, new_perm.permission_bitmap,
           to_bitmap_size(this->type) * sizeof(b64));
    return CapErrCode::SUCCESS;
}

PermissionBits PermissionBits::clone() const {
    return PermissionBits(this->basic_permissions,
                          this->permission_bitmap, this->type);
}

PermissionBits PermissionBits::allperm(PayloadType type) {
    return PermissionBits(AllPermTag{}, type);
}

PermissionBits::PermissionBits(AllPermTag, PayloadType type)
    : basic_permissions(~0ULL),
      permission_bitmap(nullptr),
      type(type) {
    size_t bitmap_size = to_bitmap_size(type);
    if (bitmap_size > 0) {
        permission_bitmap = new b64[bitmap_size];
        memset(permission_bitmap, 0xFF, bitmap_size * sizeof(b64));
    } else {
        assert(permission_bitmap == nullptr);
    }
}