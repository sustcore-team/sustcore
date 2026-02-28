/**
 * @file capability.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability Defintion
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <perm/permission.h>
#include <mem/alloc.h>
#include <sus/raii.h>
#include <sus/refcount.h>
#include <sus/rtti.h>
#include <sus/tree.h>
#include <sus/id.h>
#include <sustcore/capability.h>

#include <new>

// forward declarations

class Payload;
class Capability;
class CSpace;
class CHolder;

// 载荷基类
// 所有的载荷都必须继承自该基类
class Payload : public RTTIBase<Payload, PayloadType> {
public:
    // 说明自己的类型ID
    virtual PayloadType type_id(void) const = 0;
    // virtual destructor
    virtual ~Payload(){};
};

template <class DrvdPayload>
class _PayloadHelper : public Payload {
public:
    virtual PayloadType type_id(void) const override {
        return DrvdPayload::IDENTIFIER;
    }
};

// 能力
class Capability : public util::tree_base::TreeBase<Capability> {
protected:
    using TreeBase = util::tree_base::TreeBase<Capability>;
    // 根部Capability实际上持有这个Payload
    Payload *_payload;
    // 是否为根部Capability
    // 只有根部Capability才能删除payload
    const bool _is_root;
    // 权限位
    PermissionBits _perm;

    // 所在的CSpace, CHolder与CapIdx, 用于定位该Capability
    CSpace *_space;
    const CapIdx _idx;

    // 构造一个Capability
    // 其中, Payload被显式提供
    // 这也意味着, 这个Capability是一个根Capability, 直接管理该Payload
    Capability(Payload *payload, PermissionBits &&perm, CSpace *space,
               CapIdx idx);

    // 派生构造函数
    // 给出parent, permission bits与当前能力所在的CUniverse与index
    // 这也意味着, 这个Capability派生自parent Capability
    Capability(Capability *parent, PermissionBits &&perm, CSpace *space,
               CapIdx idx);

    // 移动构造函数
    // 给出origin与当前能力所在的CUniverse与index
    // origin的permission bits与payload将被转移到新Capability中
    Capability(Capability *origin, CSpace *space, CapIdx idx);

    static CapErrCode kill(Capability *cap);
    // 如果 murder_flag 被标记,
    // 说明该能力是通过 kill 函数被删除的
    bool murder_flag = false;

    // 析构函数
    ~Capability();

public:
    // 禁止直接new Capability
    // Capability必须被直接构造在CGroup预分配的存储空间中
    void *operator new(size_t size) = delete;
    inline void *operator new(size_t size, void *ptr) noexcept {
        return ptr;
    }
    // 同样的, Capability被删除时无需调用delete
    inline void operator delete(void *ptr) {}
    friend class CGroup;

protected:
    // 创建能力
    static Capability *create(void *ptr, Payload *payload,
                              PermissionBits &&perm, CSpace *space,
                              CapIdx idx) {
        assert(payload != nullptr);
        return new (ptr) Capability(payload, std::move(perm), space, idx);
    }

    // 派生能力
    static Capability *clone(void *ptr, Capability *parent,
                              CSpace *space,
                              CapIdx idx) {
        return new (ptr) Capability(parent, parent->perm().clone(), space, idx);
    }

    // 移动能力
    static Capability *migrate(void *ptr, Capability *origin, CSpace *space,
                               CapIdx idx) {
        return new (ptr) Capability(origin, space, idx);
    }

    CapErrCode revoke(Capability *subcap);

public:
    // 获取载荷
    constexpr Payload *raw(void) const {
        return _payload;
    }

    template <typename PT>
    PT *payload(void) const {
        return _payload->template as<PT>();
    }

    // 获取权限位
    constexpr const PermissionBits &perm(void) const {
        return _perm;
    }

    CapErrCode downgrade(const PermissionBits &new_perm) {
        assert(_perm.type == new_perm.type);
        return _perm.downgrade(new_perm);
    }

    // 获取所在的CSpace与CapIdx
    constexpr CSpace *space(void) const {
        return _space;
    }
    CHolder *holder(void) const;
    constexpr CapIdx idx(void) const {
        return _idx;
    }

    // 获取载荷类型
    constexpr PayloadType payload_type(void) const {
        assert(_payload != nullptr);
        return _payload->type_id();
    }

    constexpr bool is_root(void) const {
        return _is_root;
    }
};