/**
 * @file capdef.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief capability definition
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capdef.h>
#include <perm/permission.h>
#include <sustcore/capability.h>

// capability

Capability::Capability(Payload *payload, PermissionBits &&perm, CSpace *space,
                       CapIdx idx)
    : _payload(payload),
      _is_root(true),
      _perm(std::move(perm)),
      _space(space),
      _idx(idx) {}

Capability::Capability(Capability *parent, PermissionBits &&perm, CSpace *space,
                       CapIdx idx)
    : _payload(parent->_payload),
      _is_root(false),
      _perm(std::move(perm)),
      _space(space),
      _idx(idx) {}

Capability::Capability(Capability *origin, CSpace *space, CapIdx idx)
    : _payload(origin->_payload),
      _is_root(origin->_is_root),
      // 注意: origin的权限位被移动到新Capability中, 因此origin的权限位已被清空
      _perm(std::move(origin->_perm)),
      _space(space),
      _idx(idx) {
    origin->_payload = nullptr;
}

Capability::~Capability() {
    // 逆着继承树删除子能力

    // 根部Capability持有这个Payload
    // 因此需要删除这个Payload
    if (_is_root && _payload != nullptr)
        delete _payload;
}

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

void CGroup::_emplace_create(CSpace *space, CapIdx idx,
                             Payload *payload) {
    const size_t slot_idx = idx.slot;
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::create(place, payload, PermissionBits::allperm(payload->type_id()), space, idx);
    _slot_used[slot_idx] = true;
}

void CGroup::_emplace_clone(CSpace *space, CapIdx idx,
                             Capability *parent) {
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

CapErrCode CGroup::clone(CSpace *space, CapIdx idx,
                          Capability *parent) {
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
        CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_slot_used[slot_idx]) {
        CAPABILITY::ERROR("槽位索引%u未被占用", slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    _remove(slot_idx);
    return CapErrCode::SUCCESS;
}

CapOptional<Capability *> CGroup::get(CapIdx idx) {
    const size_t slot_idx = idx.slot;
    if (slot_idx >= CGROUP_SLOTS) {
        CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_slot_used[slot_idx]) {
        CAPABILITY::ERROR("槽位索引%u未被占用", slot_idx);
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

// CSpace
CSpace::CSpace() {
    memset(_groups, 0, sizeof(_groups));
}

CSpace::~CSpace() {
    for (size_t i = 0; i < CSPACE_SIZE; i++) {
        if (_groups[i]) {
            delete _groups[i];
        }
    }
}

CapErrCode CSpace::clone(CapIdx idx,
                          Capability *parent) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace容量", group_idx);
        return CapErrCode::INVALID_INDEX;
    }
    CGroup *group = group_at(group_idx);
    return group->clone(this, idx, parent);
}

CapErrCode CSpace::migrate(CapIdx idx, Capability *origin) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace容量", group_idx);
        return CapErrCode::INVALID_INDEX;
    }
    CGroup *group = group_at(group_idx);
    return group->migrate(this, idx, origin);
}

CapErrCode CSpace::remove(CapIdx idx) {
    const size_t group_idx = idx.group;
    if (group_idx >= CSPACE_SIZE) {
        CAPABILITY::ERROR("CGroup索引%u超出CSpace容量", group_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_groups[group_idx]) {
        CAPABILITY::ERROR("CGroup索引%u未被创建", group_idx);
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
        CAPABILITY::ERROR("CGroup索引%u超出CSpace容量", group_idx);
        return CapErrCode::INVALID_INDEX;
    }
    if (!_groups[group_idx]) {
        CAPABILITY::ERROR("CGroup索引%u未被创建", group_idx);
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

// CUniverse
CUniverse::CUniverse() {
    memset(_spaces, 0, sizeof(_spaces));
}

CUniverse::~CUniverse() {
    for (size_t i = 0; i < CUNIVERSE_SIZE; i++) {
        if (_spaces[i]) {
            delete _spaces[i];
        }
    }
}

void CUniverse::tidyup(void) {
    for (size_t i = 0; i < CUNIVERSE_SIZE; i++) {
        if (_spaces[i]) {
            _spaces[i]->tidyup();
            if (_spaces[i]->empty()) {
                delete _spaces[i];
                _spaces[i] = nullptr;
            }
        }
    }
}