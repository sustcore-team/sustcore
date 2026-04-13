/**
 * @file csa.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CSpace Accessor
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <cap/cspace.h>
#include <object/shared.h>
#include <perm/csa.h>
#include <perm/perm.h>
#include <perm/permission.h>
#include <sustcore/capability.h>

// CSpace Accessor
// 其提供了一系列接口, 用于对CSpace进行操作
// 因为CSpace的生命周期与CUniverse绑定而非与Capability绑定,
// 因此我们需要一个单独的类来访问CSpace 该类的对象生命周期与Capability绑定
class CSpaceAccessor : public _PayloadHelper<CSpaceAccessor> {
public:
    static constexpr PayloadType IDENTIFIER = PayloadType::CSPACE_ACCESSOR;

protected:
    // 该CSpaceAccessor持有一个指向CSpace的指针, 用于访问该CSpace
    CSpace *_space;

public:
    CSpaceAccessor(CSpace *space) : _space(space) {}
    ~CSpaceAccessor() = default;
    friend class CSAOp;
};

class CSAOp {
protected:
    Capability *_cap;
    CSpaceAccessor *_obj;
    CSpace *_space;

    template <b64 perm>
    bool imply(void) const {
        return _cap->perm().basic_imply(perm);
    }

    template <b64 perm>
    bool __slot_imply(size_t group_idx) const {
        using namespace perm::csa;
        const size_t offset = bitmap_offset(group_idx);
        return _cap->perm().implies<b64, SLOT_BITS>(perm, offset);
    }

    template <b64 perm>
    bool slot_imply(CapIdx idx) const {
        return __slot_imply<perm>(idx.group);
    }

public:
    constexpr CSAOp(Capability *cap)
        : _cap(cap),
          _obj(cap->payload<CSpaceAccessor>()),
          _space(_obj->_space) {
        assert(_space != nullptr);
    }
    ~CSAOp() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    template <typename PayloadType, typename... Args>
    Result<void> create(CapIdx idx, Args &&...args) {
        using namespace perm::csa;
        // 检查权限
        if (!slot_imply<SLOT_INSERT>(idx)) {
            return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
        }

        return _space->create<PayloadType>(idx, std::forward<Args>(args)...);
    }

    Result<void> clone(CapIdx dst_idx, CSpace *src_space, CapIdx src_idx);

    template <typename AccessorType>
    Result<void> split(CapIdx dst_idx, CSpace *src_space, CapIdx src_idx) {
        using namespace perm;
        using namespace csa;

        // 检查权限
        if (!slot_imply<SLOT_INSERT>(dst_idx)) {
            return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
        }

        auto cap_opt = src_space->get(src_idx);
        if (!cap_opt.has_value()) {
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }

        Capability *src_cap = cap_opt.value();
        if (!src_cap->perm().basic_imply(basic::SPLIT)) {
            return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
        }

        assert(src_cap != nullptr);
        assert(src_cap->space() == src_space);
        assert(src_cap->idx() == src_idx);

        // split 实际上是 clone 操作与 downgrade 操作针对SharedObject类型的组合

        // 首先, 从original_cap中获取原始的Accessor对象
        AccessorType *original_acc = src_cap->payload<AccessorType>();
        if (original_acc == nullptr) {
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        auto original_obj = original_acc->obj();
        // 通过create接口在dst_idx上创建一个新的Accessor对象, 该对象持有与original_obj相同的SharedObject
        Result<void> err = _space->create<AccessorType>(dst_idx, original_obj);
        if (!err.has_value()) {
            return err;
        }
        // 然后进行权限降级, 将目标Capability的权限降级为原来权限的子集
        auto dst_cap_opt = _space->get(dst_idx);
        assert(dst_cap_opt.has_value());
        Capability *dst_cap = dst_cap_opt.value();
        err = dst_cap->downgrade(src_cap->perm());
        assert(err.has_value());
        return err;
    }

    Result<void> migrate(CapIdx dst_idx, CSpace *src_space, CapIdx src_idx);
    Result<void> remove(CapIdx idx);
    Result<Capability *> lookup(CapIdx idx) const {
        using namespace perm::csa;
        if (!slot_imply<SLOT_READ>(idx)) {
            return {unexpect, ErrCode::INSUFFICIENT_PERMISSIONS};
        }
        auto cap_opt = _space->get(idx);
        if (!cap_opt.has_value()) {
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        return cap_opt.value();
    }
    Result<CapIdx> get_free_slot(void);

protected:
    CapIdx __get_free_slot(void);
};