/**
 * @file capcall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability Calls
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>

#include <cstddef>

#include "cap/permission.h"

// 使用能力时, 应当使用该头文件中提供的方法, 以确保权限检查的正确性

template <PayloadTrait Payload>
class BasicCalls {
public:
    using Cap = Capability<Payload>;

protected:
    static Payload *__payload(Cap *cap) {
        return cap->__payload();
    }
    static PermissionBits &__perm(Cap *cap) {
        return cap->_permissions;
    }

public:
    static CapOptional<Payload *> payload(Cap *cap) {
        // UNWRAP 权限校验
        if (!cap->_permissions.imply(perm::basic::UNWRAP)) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }
        return __payload(cap);
    }
};

class CSpaceCalls : public BasicCalls<CSpaceBase> {
protected:
    using Base = BasicCalls<CSpaceBase>;
    using Cap  = Base::Cap;
    static constexpr int BITMAP_SIZE =
        PermissionBits::to_bitmap_size(PermissionBits::Type::CAPSPACE);

public:
    static CapOptional<CSpaceBase *> payload(Cap *cap) {
        return Base::payload(cap);
    }

    template <PayloadTrait Payload>
    static CapErrCode insert(Cap *space_cap, size_t slot_idx,
                             Capability<Payload> *cap) {
        using namespace perm::cspace;
        // 检查权限
        if (!__perm(space_cap).imply(SLOT_INSERT, slot_offset(slot_idx))) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = __payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 插入能力对象
        space->__insert(slot_idx, cap);
        return CapErrCode::SUCCESS;
    }

    template <PayloadTrait Payload>
    static CapErrCode remove(Cap *space_cap, size_t slot_idx) {
        using namespace perm::cspace;
        // 检查权限
        if (!__perm(space_cap).imply(SLOT_REMOVE, slot_offset(slot_idx))) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = __payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 移除能力对象
        space->__remove(slot_idx);
        return CapErrCode::SUCCESS;
    }

    template <PayloadTrait Payload>
    static CapOptional<Capability<Payload> *> read(Cap *space_cap,
                                                   size_t slot_idx) {
        using namespace perm::cspace;
        // 检查权限
        if (!__perm(space_cap).imply(SLOT_READ, slot_offset(slot_idx))) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = __payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 读取能力对象
        return space->lookup(slot_idx);
    }

    template <PayloadTrait Payload>
    static CapOptional<size_t> alloc(Cap *space_cap) {
        using namespace perm::cspace;
        // 检查权限
        if (!__perm(space_cap).imply(ALLOC)) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = __payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 遍历槽位, 寻找第一个可用的空闲槽位
        for (size_t i = 0; i < BITMAP_SIZE; i++) {
            if (space->_slots[i] == nullptr &&
                __perm(space_cap).imply(SLOT_INSERT, slot_offset(i)))
            {
                // 找到一个可用的空闲槽位
                return i;
            }
        }

        return CapErrCode::INVALID_INDEX;  // 无可用槽位
    }
};