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

#include <cap/capdef.h>
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

    inline void *operator new(size_t size) noexcept {
        assert(size == sizeof(CSpaceAccessor));
        return KOP<CSpaceAccessor>::instance().alloc();
    }

    inline void operator delete(void *ptr) noexcept {
        KOP<CSpaceAccessor>::instance().free(
            static_cast<CSpaceAccessor *>(ptr));
    }

    friend class CSAOperation;
};

class CSAOperation {
protected:
    Capability *_cap;
    CSpaceAccessor *_obj;
    CSpace *_space;

    template <b64 perm>
    bool imply(void) const {
        return _cap->perm().imply(perm);
    }

    template <b64 perm>
    bool slot_imply(CapIdx idx) const {
        using namespace perm::csa;
        const size_t offset = bitmap_offset(idx.group);
        return _cap->perm().implies<b64, SLOT_BITS>(perm, offset);
    }

public:
    constexpr CSAOperation(Capability *cap)
        : _cap(cap),
          _obj(cap->payload<CSpaceAccessor>()),
          _space(_obj->_space) {
        assert(_space != nullptr);
    }
    ~CSAOperation() = default;

    void *operator new(size_t size) = delete;
    void operator delete(void *ptr) = delete;

    template <typename PayloadType, typename... Args>
    CapErrCode create(CapIdx idx, Args &&...args) {
        using namespace perm::csa;
        // 检查权限
        if (!slot_imply<SLOT_INSERT>(idx)) {
            return CapErrCode::INSUFFICIENT_PERMISSIONS;
        }

        return _space->create<PayloadType>(idx, std::forward<Args>(args)...);
    }

    CapErrCode clone(CapIdx dst_idx, CSpace *src_space, CapIdx src_idx);
    CapErrCode migrate(CapIdx dst_idx, CSpace *src_space, CapIdx src_idx);
    CapErrCode remove(CapIdx idx);
};