/**
 * @file cspace.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Capability空间
 * @version alpha-1.0.0
 * @date 2026-02-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cap/cspace.h>
#include <cap/cholder.h>
#include <sustcore/capability.h>

// CGroup

CGroup::CGroup() {
    memset(_cap_storage, 0, sizeof(_cap_storage));
    memset(_slot_used, 0, sizeof(_slot_used));
}

CGroup::~CGroup() {
    // 删除所有已使用的槽位中的Capability
    for (size_t i = 0; i < CGROUP_SLOTS; i++) {
        if (_slot_used[i]) {
            _remove(i);
        }
    }
}

void CGroup::_emplace_create(CSpace *space, CapIdx idx, Payload *payload) {
    const size_t slot_idx = idx.slot;
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::create(place, payload,
                       PermissionBits::allperm(payload->type_id()), space, idx);
    _slot_used[slot_idx] = true;
}

void CGroup::_emplace_clone(CSpace *space, CapIdx idx, Capability *parent) {
    const size_t slot_idx = idx.slot;
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::clone(place, parent, space, idx);
    _slot_used[slot_idx] = true;
}

void CGroup::_emplace_migrate(CSpace *space, CapIdx idx, Capability *origin) {
    const size_t slot_idx = idx.slot;
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::migrate(place, origin, space, idx);
    _slot_used[slot_idx] = true;
}

void CGroup::_remove(size_t slot_idx) {
    assert(slot_idx < CGROUP_SLOTS);
    assert(_slot_used[slot_idx]);
    Capability *cap =
        reinterpret_cast<Capability *>(&_cap_storage[slot_idx].data);
    delete cap;
    _slot_used[slot_idx] = false;
}

CapErrCode CGroup::clone(CSpace *space, CapIdx idx, Capability *parent) {
    const size_t slot_idx = idx.slot;
    if (slot_idx >= CGROUP_SLOTS) {
        CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (_slot_used[slot_idx]) {
        CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
        return CapErrCode::SLOT_BUSY;
    }
    _emplace_clone(space, idx, parent);
    return CapErrCode::SUCCESS;
}

CapErrCode CGroup::migrate(CSpace *space, CapIdx idx, Capability *origin) {
    const size_t slot_idx = idx.slot;
    if (slot_idx >= CGROUP_SLOTS) {
        CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (_slot_used[slot_idx]) {
        CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
        return CapErrCode::SLOT_BUSY;
    }
    _emplace_migrate(space, idx, origin);
    return CapErrCode::SUCCESS;
}

CapErrCode CGroup::remove(CapIdx idx) {
    const size_t slot_idx = idx.slot;
    if (slot_idx >= CGROUP_SLOTS) {
        CAPABILITY::ERROR("槽位索引(%u, %u)超出CGroup容量", idx.group,
                          slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_slot_used[slot_idx]) {
        CAPABILITY::ERROR("槽位索引(%u, %u)未被占用", idx.group, slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    _remove(slot_idx);
    return CapErrCode::SUCCESS;
}

CapOptional<Capability *> CGroup::get(CapIdx idx) {
    const size_t slot_idx = idx.slot;
    if (slot_idx >= CGROUP_SLOTS) {
        CAPABILITY::ERROR("槽位索引(%u, %u)超出CGroup容量", idx.group, slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_slot_used[slot_idx]) {
        CAPABILITY::ERROR("槽位索引(%u, %u)未被占用", idx.group, slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    Capability *cap =
        reinterpret_cast<Capability *>(&_cap_storage[slot_idx].data);
    return cap;
}

int CGroup::lookup_free(int last) {
    const size_t start = last < 0 ? 0 : last + 1;
    for (size_t i = start; i < CGROUP_SLOTS; i++) {
        if (!_slot_used[i]) {
            return i;
        }
    }
    return -1;
}

static util::Defer<util::IDManager<>> CSPACE_ID;
AutoDeferPost(CSPACE_ID);

// CSpace
CSpace::CSpace(CHolder *holder) :  _holder(holder), sp_idx(0) {
    memset(_groups, 0, sizeof(_groups));
}

CSpace::~CSpace() {
    for (size_t i = 0; i < CSPACE_SIZE; i++) {
        if (_groups[i]) {
            delete _groups[i];
        }
    }
}

CapErrCode CSpace::clone(CapIdx idx, Capability *parent) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return CapErrCode::INVALID_INDEX;
    }
    CGroup *group = group_at(group_idx);
    return group->clone(this, idx, parent);
}

CapErrCode CSpace::migrate(CapIdx idx, Capability *origin) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return CapErrCode::INVALID_INDEX;
    }
    CGroup *group = group_at(group_idx);
    return group->migrate(this, idx, origin);
}

CapErrCode CSpace::remove(CapIdx idx) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_groups[group_idx]) {
        CAPABILITY::ERROR("CGroup索引%u在CSpace %d中未被创建", group_idx, sp_idx);
        return CapErrCode::INVALID_INDEX;
    }
    return _groups[group_idx]->remove(idx);
}

CapOptional<CGroup *> CSpace::group(CapIdx idx) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        return CapErrCode::INVALID_INDEX;
    }
    if (!_groups[group_idx]) {
        return CapErrCode::INVALID_INDEX;
    }
    return _groups[group_idx];
}

CapOptional<Capability *> CSpace::get(CapIdx idx) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_groups[group_idx]) {
        CAPABILITY::ERROR("CGroup索引%u在CSpace %d中未被创建", group_idx, sp_idx);
        return CapErrCode::INVALID_INDEX;
    }
    return _groups[group_idx]->get(idx);
}

void CSpace::tidyup(void) {
    for (size_t i = 0; i < CSPACE_SIZE; i++) {
        if (_groups[i] && _groups[i]->empty()) {
            delete _groups[i];
            _groups[i] = nullptr;
        }
    }
}

// RecvSpace
RecvSpace::RecvSpace(CHolder *holder) : CSpace(holder) {
    memset(_groups, 0, sizeof(_groups));
}

CapErrCode RecvSpace::migrate(CapIdx idx, Capability *origin) {
    // 进行一个检验
    if (origin->holder()->cholder_id != _recv_src[idx.group]) {
        CAPABILITY::ERROR("无法接收从CHolder %d迁移过来的能力: 接收空间的recv_src不匹配",
                          _recv_src[idx.group]);
        return CapErrCode::INVALID_INDEX;
    }
    return CSpace::migrate(idx, origin);
}