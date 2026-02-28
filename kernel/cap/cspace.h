/**
 * @file cspace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability空间
 * @version alpha-1.0.0
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cap/capability.h>

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

    friend class CSAOperation;
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

    friend class CSAOperation;
};

class RecvSpace : protected CSpace {
protected:
    // 记录每个group的能力来源, 以便在接收迁移过来的能力时进行权限检查
    // 只有当recv_src[group_idx] == src_cholder_id时, 才允许接收从src_cholder迁移过来的能力
    size_t _recv_src[CSPACE_SIZE];
public:
    RecvSpace(CHolder *holder);

    using CSpace::remove;
    using CSpace::get;
    using CSpace::group;
    using CSpace::empty;
    using CSpace::tidyup;

    CapErrCode migrate(CapIdx idx, Capability *origin);
    inline void set_sender(size_t group_idx, size_t src_cholder_id) {
        assert(group_idx < CSPACE_SIZE);
        _recv_src[group_idx] = src_cholder_id;
    }

    friend class CSAOperation;
};