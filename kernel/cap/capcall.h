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
        // CCALL::basic::create() 函数
        // 比较复杂, 暂时先不对其进行约束
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

        using CreateArgs = std::tuple<CSpaceBase *, CapHolder *, size_t,
                                      const PermissionBits &>;
        static CapOptional<Cap *> create(CSpaceBase *space, CapHolder *owner,
                                         CapIdx idx,
                                         const PermissionBits &bits);
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

    static constexpr CapHolder *owner(Cap *space_cap) {
        return _payload(space_cap)->_holder;
    }
    static constexpr size_t index(Cap *space_cap) {
        return _payload(space_cap)->index();
    }

    template <ExtendedPayloadTrait Payload>
    static CapErrCode clone(Cap *dst_space, CapIdx dst_idx,
                            CapHolder *src_owner, CapIdx src_idx) {
        using BC   = Payload::CCALL::basic;
        using CapP = Capability<Payload>;

        // 检测space_cap是否来自于dst_owner
        if (index(dst_space) != dst_idx.space) {
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
            BC::clone(src_opt.value(), owner(dst_space), dst_idx);
        if (!cloned_opt.present()) {
            return cloned_opt.error();
        }

        // 插入到dst中
        CapErrCode code = insert(dst_space, dst_idx.slot, cloned_opt.value());
        if (code != CapErrCode::SUCCESS) {
            // 其实可以用RAII的, 但是, 就这一个分支, 还是手动清理比较好
            delete cloned_opt.value();
            return code;
        }
        // 成功插入后, 接受该能力
        cloned_opt.value()->accept();
        return CapErrCode::SUCCESS;
    }

    template <ExtendedPayloadTrait Payload>
    static CapErrCode migrate(Cap *dst_space, CapIdx dst_idx,
                              CapHolder *src_owner, CapIdx src_idx) {
        using BC   = Payload::CCALL::basic;
        using CapP = Capability<Payload>;

        // 检测space_cap是否来自于dst_owner
        if (index(dst_space) != dst_idx.space) {
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
            BC::migrate(src_opt.value(), owner(dst_space), dst_idx);
        if (!migrated_opt.present()) {
            return migrated_opt.error();
        }

        // 插入到dst中
        CapErrCode code = insert(dst_space, dst_idx.slot, migrated_opt.value());
        if (code != CapErrCode::SUCCESS) {
            // 其实可以用RAII的, 但是, 就这一个分支, 还是手动清理比较好
            delete migrated_opt.value();
            return code;
        }
        // 成功插入后, 接受该能力
        migrated_opt.value()->accept();

        // 从源holder中删除该能力
        CapOptional<CSpace<Payload> *> sp_opt =
            src_owner->lookup_space<Payload>(src_idx.space);
        // 逻辑上, 源索引是一定存在的
        // 我们在此处添加assert以验证这一点
        assert(sp_opt.present());
        sp_opt.value()->__remove(src_idx.slot);

        // migrate方法中, 迁移后的能力已经替代了迁移前的能力
        // 我们可以将其discard并删除
        src_opt.value()->discard();
        delete src_opt.value();

        return CapErrCode::SUCCESS;
    }

    // Should be called by the source owner only
    template <ExtendedPayloadTrait Payload>
    static CapOptional<size_t> try_migrate(CapHolder *dst_owner, CapIdx dst_idx,
                                           CapHolder *src_owner,
                                           CapIdx src_idx) {
        using CapP = Capability<Payload>;

        // 首先, 获得源能力
        CapOptional<CapP *> src_opt = src_owner->lookup<Payload>(src_idx);
        if (!src_opt.present()) {
            return src_opt.error();
        }

        // 构造migrate token
        size_t mt_idx = src_owner->template create_migrate_token<Payload>(
            src_idx, dst_owner);
        return mt_idx;
    }

    // Should be called by the destination owner only
    template <ExtendedPayloadTrait Payload>
    static CapErrCode accept_migrate(Cap *dst_space, CapIdx dst_idx,
                                     CapHolder *src_owner, size_t mt_idx) {
        using namespace perm::cspace;
        using MTok = typename CapHolder::template MigrateToken<Payload>;

        // 通过mt_idx获取migrate token
        MTok token = src_owner->template peer_migrate_token<Payload>(mt_idx);
        if (token.src_idx.nullable()) {
            return CapErrCode::INVALID_INDEX;
        }

        if (token.dst_holder != owner(dst_space)) {
            return CapErrCode::INVALID_CAPABILITY;
        }

        // 然后调用migrate方法进行能力迁移
        CapErrCode ret = migrate(dst_space, dst_idx, src_owner, token.src_idx);
        if (ret == CapErrCode::SUCCESS) {
            src_owner->template fetch_migrate_token<Payload>(
            mt_idx);  // clear the token
        }

        return ret;
    }

    template <ExtendedPayloadTrait Payload, typename... Args>
    static CapErrCode create(Cap *space_cap, CapHolder *owner, CapIdx idx,
                             const PermissionBits &bits, Args &&...args) {
        using namespace perm::cspace;
        using BC = Payload::CCALL::basic;
        // 检查权限
        if (!imply<ALLOC>(space_cap)) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        // 获得space_cap的Payload
        CSpace<Payload> *space = _payload(space_cap)->as<CSpace<Payload>>();
        if (space == nullptr) {
            return CapErrCode::TYPE_NOT_MATCHED;
        }

        // 创建能力对象
        auto cap_opt =
            BC::create(std::forward<Args>(args)..., owner, idx, bits);
        if (!cap_opt.present()) {
            return cap_opt.error();
        }

        Capability<Payload> *cap = cap_opt.value();

        // 插入到dst中
        CapErrCode code = insert(space_cap, idx.slot, cap);
        if (code != CapErrCode::SUCCESS) {
            // 如果失败则将其删除
            delete cap;
            return code;
        }
        // 成功插入后, 接受该能力
        cap->accept();
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
        for (size_t i = 0; i < CSpace<Payload>::SpaceSize; i++) {
            if (space->_slots[i] == nullptr &&
                _perm(space_cap).imply(SLOT_INSERT, slot_offset(i)))
            {
                // 找到一个可用的空闲槽位
                // 额外检验: 其索引不为(0, 0)
                if (space->index() + i == 0) {
                    continue;  // 跳过(0, 0)槽位
                }
                return i;
            }
        }

        return CapErrCode::INVALID_INDEX;  // 无可用槽位
    }
};

static_assert(CapCallTrait<CSpaceCalls, CSpaceBase>);