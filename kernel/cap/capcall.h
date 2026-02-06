/**
 * @file capcall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability Calls
 * @version alpha-1.0.0
 * @date 2026-02-06
 *
 * 使用能力时, 应当使用该头文件中提供的方法, 以确保权限检查的正确性
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <cap/capdef.h>
#include <cap/permission.h>
#include <sustcore/cap_type.h>

#include <cstddef>

// CapCall Trait
template <typename CCALL, typename Payload>
concept CapCallTrait =
    PayloadTrait<Payload> &&
    requires(Capability<Payload> *cap, CapHolder *owner, CapIdx idx,
             const PermissionBits &new_perm) {
        {
            CCALL::basic::payload(cap)
        } -> std::same_as<CapOptional<Payload *>>;
        {
            CCALL::basic::clone(cap, owner, idx)
        } -> std::same_as<CapOptional<Capability<Payload> *>>;
        {
            CCALL::basic::migrate(cap, owner, idx)
        } -> std::same_as<CapOptional<Capability<Payload> *>>;
        {
            CCALL::basic::downgrade(cap, new_perm)
        } -> std::same_as<CapErrCode>;
    };

template <typename Payload>
concept ExtendedPayloadTrait =
    PayloadTrait<Payload> && CapCallTrait<typename Payload::CCALL, Payload>;

template <PayloadTrait Payload>
class BasicCalls {
public:
    using Cap = Capability<Payload>;

protected:
    static Payload *_payload(Cap *cap) {
        return cap->_payload;
    }
    static const PermissionBits &_perm(const Cap *cap) {
        return cap->_permissions;
    }
    static PermissionBits &_perm(Cap *cap) {
        return cap->_permissions;
    }

    template <b64 PERM>
    static constexpr bool imply(Cap *cap, int offset) {
        return cap->_permissions.imply(PERM, offset);
    }

    template <b64 PERM>
    static constexpr bool imply(Cap *cap) {
        return cap->_permissions.imply(PERM);
    }

public:
    struct basic {
        static CapOptional<Payload *> payload(Cap *cap) {
            using namespace perm::basic;
            // UNWRAP 权限校验
            if (!imply<UNWRAP>(cap)) {
                return CapErrCode::INSUFFICIENT_PERMISSIONS;
            }
            return _payload(cap);
        }
    };
};

class CSpaceCalls : public BasicCalls<CSpaceBase> {
protected:
    using Base = BasicCalls<CSpaceBase>;
    using Cap  = Base::Cap;
    static constexpr int BITMAP_SIZE =
        PermissionBits::to_bitmap_size(PermissionBits::Type::CAPSPACE);
    static Cap *_clone(Cap *src, CapHolder *owner, CapIdx idx);
    static Cap *_migrate(Cap *src, CapHolder *owner, CapIdx idx);

public:
    // Basic Operations
    struct basic {
        static CapOptional<CSpaceBase *> payload(Cap *cap);
        static CapOptional<Cap *> clone(Cap *src, CapHolder *owner, CapIdx idx);
        static CapOptional<Cap *> migrate(Cap *src, CapHolder *owner,
                                          CapIdx idx);
        static CapErrCode downgrade(Cap *src, const PermissionBits &new_perm);
    };

    // CSpace Operations
    template <ExtendedPayloadTrait Payload>
    static CapErrCode insert(Cap *space_cap, size_t slot_idx,
                             Capability<Payload> *cap) {
        using namespace perm::cspace;
        // 检查权限
        if (!imply<SLOT_INSERT>(space_cap, slot_offset(slot_idx))) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = _payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 插入能力对象
        space->__insert(slot_idx, cap);
        return CapErrCode::SUCCESS;
    }

    template <ExtendedPayloadTrait Payload>
    static CapErrCode remove(Cap *space_cap, size_t slot_idx) {
        using namespace perm::cspace;
        // 检查权限
        if (!imply<SLOT_REMOVE>(space_cap, slot_offset(slot_idx))) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = _payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 移除能力对象
        space->__remove(slot_idx);
        return CapErrCode::SUCCESS;
    }

    template <ExtendedPayloadTrait Payload>
    static CapOptional<Capability<Payload> *> read(Cap *space_cap,
                                                   size_t slot_idx) {
        using namespace perm::cspace;
        // 检查权限
        if (!imply<SLOT_READ>(space_cap, slot_offset(slot_idx))) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = _payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 读取能力对象
        return space->lookup(slot_idx);
    }

    static bool from(Cap *space_cap, size_t space_idx, const CapHolder *owner);
    static size_t index(Cap *space_cap);

    template <ExtendedPayloadTrait Payload>
    static CapErrCode clone(Cap *dst_space, CapHolder *dst_owner,
                            CapIdx dst_idx, CapHolder *src_owner,
                            CapIdx src_idx) {
        using BC   = Payload::CCALL::basic;
        using CapP = Capability<Payload>;

        // 检测space_cap是否来自于dst_owner
        if (!from(dst_space, dst_idx.space, dst_owner)) {
            return CapErrCode::INVALID_CAPABILITY;
        }

        // 我们不需要做权限检测, insert方法和BC::clone方法会帮我们做的
        // 首先, 获得源能力
        CapOptional<CapP *> src_opt = src_owner->lookup<Payload>(src_idx);
        if (!src_opt.present()) {
            return src_opt.error();
        }

        // 复制源能力
        CapOptional<CapP *> cloned_opt =
            BC::clone(src_opt.value(), dst_owner, dst_idx);
        if (!cloned_opt.present()) {
            return cloned_opt.error();
        }

        // 插入到dst中
        CapErrCode code = insert(dst_space, dst_idx.slot, cloned_opt.value());
        if (code != CapErrCode::SUCCESS) {
            // 其实可以用RAII的, 但是, 就这一个分支, 还是手动清理比较好
            cloned_opt.value()->discard();
            delete cloned_opt.value();
        }
        return code;
    }

    template <ExtendedPayloadTrait Payload>
    static CapErrCode migrate(Cap *dst_space, CapHolder *dst_owner,
                              CapIdx dst_idx, CapHolder *src_owner,
                              CapIdx src_idx) {
        using BC   = Payload::CCALL::basic;
        using CapP = Capability<Payload>;

        // 检测space_cap是否来自于dst_owner
        if (!from(dst_space, dst_idx.space, dst_owner)) {
            return CapErrCode::INVALID_CAPABILITY;
        }

        // 我们不需要做权限检测, migrate方法和BC::clone方法会帮我们做的
        // 首先, 获得源能力
        CapOptional<CapP *> src_opt = src_owner->lookup<Payload>(src_idx);
        if (!src_opt.present()) {
            return src_opt.error();
        }

        // 迁移源能力
        CapOptional<CapP *> migrated_opt =
            BC::migrate(src_opt.value(), dst_owner, dst_idx);
        if (!migrated_opt.present()) {
            return migrated_opt.error();
        }

        // 插入到dst中
        CapErrCode code = insert(dst_space, dst_idx.slot, migrated_opt.value());
        if (code != CapErrCode::SUCCESS) {
            // 其实可以用RAII的, 但是, 就这一个分支, 还是手动清理比较好
            migrated_opt.value()->discard();
            delete migrated_opt.value();
            return code;
        }

        // 从源holder中删除该能力
        CapOptional<CSpace<Payload> *> sp_opt =
            src_owner->space<Payload>(src_idx.space);
        // assert(sp_opt.present());
        sp_opt.value()->__remove(src_idx.slot);

        // migrate方法中, 迁移后的能力已经替代了迁移前的能力
        // 我们可以将其discard并删除
        src_opt.value()->discard();
        delete src_opt.value();

        return CapErrCode::SUCCESS;
    }

    template <ExtendedPayloadTrait Payload>
    static CapOptional<size_t> alloc(Cap *space_cap) {
        using namespace perm::cspace;
        // 检查权限
        if (!imply<ALLOC>(space_cap)) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = _payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 遍历槽位, 寻找第一个可用的空闲槽位
        for (size_t i = 0; i < BITMAP_SIZE; i++) {
            if (space->_slots[i] == nullptr &&
                _perm(space_cap).imply(SLOT_INSERT, slot_offset(i)))
            {
                // 找到一个可用的空闲槽位
                return i;
            }
        }

        return CapErrCode::INVALID_INDEX;  // 无可用槽位
    }
};

static_assert(CapCallTrait<CSpaceCalls, CSpaceBase>);