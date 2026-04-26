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
#include <sus/owner.h>
#include <sus/raii.h>
#include <sus/refcount.h>
#include <sus/rtti.h>
#include <sus/tree.h>
#include <sustcore/capability.h>

#include <new>

// forward declarations

class Payload;
class Capability;
class CGroup;
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

    // 所在的CGroup与CapIdx, 用于定位该Capability
    CGroup *_group;
    const CapIdx _idx;

    // 构造一个Capability
    // 其中, Payload被显式提供
    // 这也意味着, 这个Capability是一个根Capability, 直接管理该Payload
    Capability(util::owner<Payload *> payload, PermissionBits &&perm, CGroup *group,
               CapIdx idx);

    // 派生构造函数
    // 给出parent, permission bits与当前能力所在的CUniverse与index
    // 这也意味着, 这个Capability派生自parent Capability
    Capability(Capability *parent, PermissionBits &&perm, CGroup *group,
               CapIdx idx);

    // 移动构造函数
    // 给出origin与当前能力所在的CUniverse与index
    // origin的permission bits与payload将被转移到新Capability中
    Capability(Capability *origin, CGroup *group, CapIdx idx);

    static Result<void> kill(Capability *cap);
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
    static Capability *create(void *ptr, util::owner<Payload *> payload,
                              PermissionBits &&perm, CGroup *group,
                              CapIdx idx) {
        assert(payload != nullptr);
        return new (ptr) Capability(payload, std::move(perm), group, idx);
    }

    // 派生能力
    static Capability *clone(void *ptr, Capability *parent,
                              CGroup *group,
                              CapIdx idx) {
        return new (ptr) Capability(parent, parent->perm().clone(), group, idx);
    }

    // 移动能力
    static Capability *migrate(void *ptr, Capability *origin, CGroup *group,
                               CapIdx idx) {
        return new (ptr) Capability(origin, group, idx);
    }

    Result<void> revoke(Capability *subcap);

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

    Result<void> downgrade(const PermissionBits &new_perm) {
        assert(_perm.type == new_perm.type);
        return _perm.downgrade(new_perm);
    }

    // 获取所在的CGroup与CapIdx
    constexpr CGroup *group(void) const {
        return _group;
    }

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

// CGroup 是存放一组 Capability 的基本单元
class CGroup {
public:
    struct PreCap {
        alignas(Capability) char data[sizeof(Capability)];
    };
    static_assert(sizeof(PreCap) == sizeof(Capability),
                  "PreCap must be the same size as Capability");
    // 预分配的Capability存储空间
    // 该CGroup中每个槽位的使用情况
    PreCap _cap_storage[CGROUP_SLOTS];
    bool _slot_used[CGROUP_SLOTS];

    // 构造/删除Capability
    void _emplace_create(CapIdx idx, util::owner<Payload *> payload);
    void _emplace_clone(CapIdx idx, Capability *parent);
    void _emplace_migrate(CapIdx idx, Capability *origin);
    void _remove(size_t slot_idx);

public:
    CGroup();
    ~CGroup();

    // 每个Payload的生命周期都与一个Capability绑定
    // 因此, Payload产生时, Capability也随之产生; Payload销毁时,
    // Capability也随之销毁
    template <typename PayloadType, typename... Args>
    Result<void> create(CapIdx idx, Args &&...args) {
        const size_t slot_idx = capidx::slot(idx);
        if (slot_idx >= CGROUP_SLOTS) {
            loggers::CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        if (_slot_used[slot_idx]) {
            loggers::CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
            return {unexpect, ErrCode::SLOT_BUSY};
        }
        // 直接构造Payload
        auto payload = util::owner(new PayloadType(std::forward<Args>(args)...));
        _emplace_create(idx, payload);
        return {};
    }

    template <typename PayloadType>
    Result<void> create_from(CapIdx idx, util::owner<Payload *> payload) {
        const size_t slot_idx = capidx::slot(idx);
        if (slot_idx >= CGROUP_SLOTS) {
            loggers::CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        if (_slot_used[slot_idx]) {
            loggers::CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
            return {unexpect, ErrCode::SLOT_BUSY};
        }
        _emplace_create(idx, payload);
        return {};
    }

    Result<void> clone(CapIdx idx, Capability *parent);
    Result<void> migrate(CapIdx idx, Capability *origin);
    Result<void> remove(CapIdx idx);

    Result<Capability *> get(CapIdx idx);
    // 寻找自last开始的下一个空闲的槽位
    // 若没有, 返回-1
    int lookup_free(int last = -1);

    // 通过KOP分配回收CGroup
    void *operator new(size_t size);
    void operator delete(void *ptr);

    constexpr bool empty(void) const {
        bool flag = false;
        for (size_t i = 0; i < CGROUP_SLOTS; i++) {
            flag |= _slot_used[i];
        }
        return !flag;
    }

    friend class CSAOperator;
};


// CSpace
// 每个CSpace都含有若干个CGroup
// 然而这些CGroup并不一定一开始都被创建
// 只有被使用时, CGroup才会被创建
class CSpace {
protected:
    CGroup *_groups[CSPACE_SIZE];
    CHolder *_holder;

    inline CGroup *group_at(size_t group_idx) {
        assert(group_idx < CSPACE_SIZE);
        if (_groups[group_idx] == nullptr) {
            _groups[group_idx] = new CGroup();
        }

        return _groups[group_idx];
    }

public:
    const int sp_idx;
    CSpace(CHolder *holder);
    ~CSpace();

    constexpr CHolder *holder(void) const {
        return _holder;
    }

    template <typename PayloadType, typename... Args>
    Result<void> create(CapIdx idx, Args &&...args) {
        const size_t group_idx = capidx::group(idx);
        if (group_idx >= CSPACE_SIZE) {
            loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx,
                              this->sp_idx);
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        CGroup *group = group_at(group_idx);
        return group->create<PayloadType>(idx,
                                          std::forward<Args>(args)...);
    }

    template <typename PayloadType>
    Result<void> create_from(CapIdx idx, util::owner<Payload *> payload) {
        const size_t group_idx = capidx::group(idx);
        if (group_idx >= CSPACE_SIZE) {
            loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx,
                              this->sp_idx);
            return {unexpect, ErrCode::OUT_OF_BOUNDARY};
        }
        CGroup *group = group_at(group_idx);
        return group->create_from<PayloadType>(idx, payload);
    }

    Result<void> clone(CapIdx idx, Capability *parent);
    Result<void> migrate(CapIdx idx, Capability *origin);
    Result<void> remove(CapIdx idx);

    // get group
    Result<CGroup *> group(CapIdx idx);
    Result<Capability *> get(CapIdx idx);

    constexpr bool empty(void) const {
        bool flag = false;
        for (size_t i = 0; i < CSPACE_SIZE; i++) {
            flag |= (_groups[i] != nullptr);
        }
        return !flag;
    }
    // 腾出CSpace中所有为空的CGroup
    void tidyup(void);

    friend class CSAOperator;
};