/**
 * @file capdef.h
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
#include <sustcore/capability.h>

#include <new>

// forward declarations

class Payload;
class Capability;
class CSpace;

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
class Capability : public util::tree_base::Tree<Capability> {
protected:
    using TreeBase = util::tree_base::Tree<Capability>;
    // 根部Capability实际上持有这个Payload
    Payload *_payload;
    // 是否为根部Capability
    // 只有根部Capability才能删除payload
    const bool _is_root;
    // 权限位
    PermissionBits _perm;

    // 所在的CSpace与CapIdx, 用于定位该Capability
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

// CGroup
// CGroup是CSpace中的一个容器, 用于存放Capability
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
    void _emplace_create(CSpace *space, CapIdx idx,
                         Payload *payload);
    void _emplace_clone(CSpace *space, CapIdx idx,
                         Capability *parent);
    void _emplace_migrate(CSpace *space, CapIdx idx, Capability *origin);
    void _remove(size_t slot_idx);

public:
    CGroup();
    ~CGroup();

    // 每个Payload的生命周期都与一个Capability绑定
    // 因此, Payload产生时, Capability也随之产生; Payload销毁时,
    // Capability也随之销毁
    template <typename PayloadType, typename... Args>
    CapErrCode create(CSpace *space, CapIdx idx,
                      Args &&...args) {
        const size_t slot_idx = idx.slot;
        if (slot_idx >= CGROUP_SLOTS) {
            CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
            return CapErrCode::INVALID_INDEX;
        }
        if (_slot_used[slot_idx]) {
            CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
            return CapErrCode::SLOT_BUSY;
        }
        // 直接构造Payload
        Payload *payload = new PayloadType(std::forward<Args>(args)...);
        _emplace_create(space, idx, payload);
        return CapErrCode::SUCCESS;
    }

    CapErrCode clone(CSpace *space, CapIdx idx,
                      Capability *parent);
    CapErrCode migrate(CSpace *space, CapIdx idx, Capability *origin);
    CapErrCode remove(CapIdx idx);

    CapOptional<Capability *> get(CapIdx idx);
    // 寻找自last开始的下一个空闲的槽位
    // 若没有, 返回-1
    int lookup_free(int last = -1);

    // 通过KOP分配CGroup实例
    inline void *operator new(size_t size) {
        assert(size == sizeof(CGroup));
        return KOP<CGroup>::instance().alloc();
    }

    // 通过KOP删除CGroup实例
    inline void operator delete(void *ptr) {
        KOP<CGroup>::instance().free(static_cast<CGroup *>(ptr));
    }

    constexpr bool empty(void) const {
        bool flag = false;
        for (size_t i = 0; i < CGROUP_SLOTS; i++) {
            flag |= _slot_used[i];
        }
        return ! flag;
    }
};

// CSpace
// 每个CSpace都含有若干个CGroup
// 然而这些CGroup并不一定一开始都被创建
// 只有被使用时, CGroup才会被创建
class CSpace {
protected:
    CGroup *_groups[CSPACE_SIZE];

    inline CGroup *group_at(size_t group_idx) {
        assert(group_idx < CSPACE_SIZE);
        if (_groups[group_idx] == nullptr) {
            _groups[group_idx] = new CGroup();
        }

        return _groups[group_idx];
    }

public:
    const size_t sp_idx;
    CSpace(size_t sp_idx);
    ~CSpace();

    template <typename PayloadType, typename... Args>
    CapErrCode create(CapIdx idx, Args &&...args) {
        const size_t group_idx = idx.group;
        if (group_idx >= CSPACE_SIZE) {
            CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, this->sp_idx);
            return CapErrCode::INVALID_INDEX;
        }
        CGroup *group = group_at(group_idx);
        return group->create<PayloadType>(this, idx,
                                          std::forward<Args>(args)...);
    }
    CapErrCode clone(CapIdx idx, Capability *parent);
    CapErrCode migrate(CapIdx idx, Capability *origin);
    CapErrCode remove(CapIdx idx);

    // get group
    CapOptional<CGroup *> group(CapIdx idx);
    CapOptional<Capability *> get(CapIdx idx);

    constexpr bool empty(void) const {
        bool flag = false;
        for (size_t i = 0; i < CSPACE_SIZE; i++) {
            flag |= (_groups[i] != nullptr);
        }
        return !flag;
    }
    // 腾出CSpace中所有为空的CGroup
    void tidyup(void);
};

class CUniverse {
protected:
    CSpace *_spaces[CUNIVERSE_SIZE];
    int lookup_free_space(void) {
        for (size_t i = 0; i < CUNIVERSE_SIZE; i++) {
            if (_spaces[i] == nullptr) {
                return i;
            }
        }
        return -1;  // indicate no free space
    }
public:
    CUniverse();
    ~CUniverse();

    CSpace *new_space(void) {
        const int idx = lookup_free_space();
        if (idx < 0) {
            CAPABILITY::ERROR("CUniverse空间已满, 无法创建新的CSpace");
            return nullptr;
        }
        _spaces[idx] = new CSpace(idx);
        return _spaces[idx];
    }

    CSpace *space(int idx) const {
        if (idx < 0 || idx >= CUNIVERSE_SIZE) {
            CAPABILITY::ERROR("CSpace索引%u超出CUniverse容量", idx);
            return nullptr;
        }
        return _spaces[idx];
    }

    // 腾出CUniverse中所有为空的CSpace
    void tidyup(void);
};

// CHolder是抽象的能力持有者, 例如线程, 进程等
// 其拥有一个指向CUniverse的指针, 与两个space idx, 分别指向major space与minor space
class CHolder {
protected:
    CUniverse *_universe;
    int _major_space_idx;
    int _minor_space_idx;
public:
    CHolder(CUniverse *universe, int major_space_idx = -1, int minor_space_idx = -1)
        : _universe(universe), _major_space_idx(major_space_idx),
          _minor_space_idx(minor_space_idx) {}
    CSpace *major_space(void) const {
        if (_major_space_idx < 0) {
            return nullptr;
        }
        return _universe->space(_major_space_idx);
    }
    CSpace *minor_space(void) const {
        if (_minor_space_idx < 0) {
            return nullptr;
        }
        return _universe->space(_minor_space_idx);
    }
    CUniverse *universe(void) const {
        return _universe;
    }
};