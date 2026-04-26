/**
 * @file capability.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief capability definition
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/capability.h>
#include <kio.h>
#include <perm/permission.h>
#include <sus/defer.h>
#include <sus/list.h>
#include <sus/queue.h>
#include <sustcore/capability.h>

#include <cassert>

// capability

Capability::Capability(util::owner<Payload *> payload, PermissionBits &&perm,
                       CGroup *group, CapIdx idx)
    : TreeBase(),
      _payload(payload),
      _is_root(true),
      _perm(std::move(perm)),
      _group(group),
      _idx(idx) {}

Capability::Capability(Capability *parent, PermissionBits &&perm, CGroup *group,
                       CapIdx idx)
    : TreeBase(parent),
      _payload(parent->_payload),
      _is_root(false),
      _perm(std::move(perm)),
      _group(group),
      _idx(idx) {}

Capability::Capability(Capability *origin, CGroup *group, CapIdx idx)
    : TreeBase(origin->parent, std::move(origin->children)),
      _payload(origin->_payload),
      _is_root(origin->_is_root),
      // 注意: origin的权限位被移动到新Capability中, 因此origin的权限位已被清空
      _perm(std::move(origin->_perm)),
      _group(group),
      _idx(idx) {
    origin->_payload = nullptr;

    // 让自身替换origin在继承树中的位置
    // 此时已经通过移动语义将其内容搬走了
    // 我们通过clear()使得origin->children可以被再次使用
    origin->children.clear();
    if (this->parent != nullptr) {
        // 将origin从父节点的子节点列表中移除
        this->parent->remove_child(origin);
    }
    // 遍历子节点并设置新父亲
    for (auto &child : children) {
        child->set_parent(this);
    }
}

// NOTE: 当派生树深度过大时, 递归删除可能会导致栈溢出
Result<void> Capability::kill(Capability *cap) {
    assert(cap != nullptr);
    // 定位到cap所在的CSpace与Index
    CGroup *grp = cap->_group;
    assert(grp != nullptr);
    CapIdx idx = cap->_idx;
    assert(capidx::valid(idx));
    cap->murder_flag = true;
    return grp->remove(idx);
}

// 构造工作队列
// 至多允许在队列中存储4096个准备删除的能力
// TODO: 允许队列动态扩容, 或是限制能力能够派生的最大子能力数目
// 实际上, 我们只需要追踪根能力的子能力数目就好了
static constexpr size_t WORK_QUEUE_SIZE = 4096;
static util::StaticArrayQueue<Capability *, WORK_QUEUE_SIZE> __working_queue;

// 注: 删除能力应该是一个原子操作
Capability::~Capability() {
    if (murder_flag) {
        return;
    }

    // 声明将亡
    loggers::CAPABILITY::DEBUG("开始移除(%d, %d)",
                               capidx::group(this->_idx),
                               capidx::slot(this->_idx));
    // 通知父节点移除自己
    if (parent != nullptr) {
        loggers::CAPABILITY::DEBUG("从(%d, %d)中移除自身",
                       capidx::group(parent->_idx),
                       capidx::slot(parent->_idx));
        parent->remove_child(this);
    }

    // 删除所有的子能力
    __working_queue.clear();
    for (auto &subcap : children) {
        __working_queue.push(subcap);
    }

    // 通过广度优先搜索删除其子能力
    while (!__working_queue.empty()) {
        // 取出头部的能力
        Capability *cur = __working_queue.front();
        __working_queue.pop();

        // 加入该能力的直接子能力
        for (auto &subcap : cur->children) {
            __working_queue.push(subcap);
            loggers::CAPABILITY::DEBUG(
                "加入工作队列: (%d, %d)", capidx::group(subcap->_idx),
                capidx::slot(subcap->_idx));
        }

        // 删除该能力
        loggers::CAPABILITY::DEBUG(
            "移除(%d, %d)", capidx::group(cur->_idx),
            capidx::slot(cur->_idx));
        kill(cur);
    }

    // 根部Capability持有这个Payload
    // 因此需要删除这个Payload
    if (_is_root && _payload != nullptr) {
        loggers::CAPABILITY::DEBUG("Deleting payloads");
        delete _payload;
    }
}

Result<void> Capability::revoke(Capability *subcap) {
    if (subcap->parent != this) {
        loggers::CAPABILITY::ERROR("无法撤销非直接子能力");
        return {unexpect, ErrCode::INVALID_CAPABILITY};
    }
    // 将subcap从子节点列表中移除
    return kill(subcap);
}

// 内存池

namespace kop {
    util::Defer<KOP<CGroup>> CGroup;
    AutoDefer(CGroup);
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

void CGroup::_emplace_create(CapIdx idx, util::owner<Payload *> payload) {
    const size_t slot_idx = capidx::slot(idx);
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::create(place, payload,
                       PermissionBits::allperm(payload->type_id()), this, idx);
    _slot_used[slot_idx] = true;
}

void CGroup::_emplace_clone(CapIdx idx, Capability *parent) {
    const size_t slot_idx = capidx::slot(idx);
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::clone(place, parent, this, idx);
    _slot_used[slot_idx] = true;
}

void CGroup::_emplace_migrate(CapIdx idx, Capability *origin) {
    const size_t slot_idx = capidx::slot(idx);
    assert(slot_idx < CGROUP_SLOTS);
    assert(!_slot_used[slot_idx]);
    void *place = &_cap_storage[slot_idx].data;
    Capability::migrate(place, origin, this, idx);
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

Result<void> CGroup::clone(CapIdx idx, Capability *parent) {
    const size_t slot_idx = capidx::slot(idx);
    if (slot_idx >= CGROUP_SLOTS) {
        loggers::CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    if (_slot_used[slot_idx]) {
        loggers::CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
        return {unexpect, ErrCode::SLOT_BUSY};
    }
    _emplace_clone(idx, parent);
    return {};
}

Result<void> CGroup::migrate(CapIdx idx, Capability *origin) {
    const size_t slot_idx = capidx::slot(idx);
    if (slot_idx >= CGROUP_SLOTS) {
        loggers::CAPABILITY::ERROR("槽位索引%u超出CGroup容量", slot_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    if (_slot_used[slot_idx]) {
        loggers::CAPABILITY::ERROR("槽位索引%u已被占用", slot_idx);
        return {unexpect, ErrCode::SLOT_BUSY};
    }
    _emplace_migrate(idx, origin);
    return {};
}

Result<void> CGroup::remove(CapIdx idx) {
    const size_t slot_idx = capidx::slot(idx);
    if (slot_idx >= CGROUP_SLOTS) {
        loggers::CAPABILITY::ERROR("槽位索引(%u, %u)超出CGroup容量", capidx::group(idx),
                          slot_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_slot_used[slot_idx]) {
        loggers::CAPABILITY::ERROR("槽位索引(%u, %u)未被占用", capidx::group(idx), slot_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    _remove(slot_idx);
    return {};
}

Result<Capability *> CGroup::get(CapIdx idx) {
    const size_t slot_idx = capidx::slot(idx);
    if (slot_idx >= CGROUP_SLOTS) {
        loggers::CAPABILITY::ERROR("槽位索引(%u, %u)超出CGroup容量", capidx::group(idx), slot_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_slot_used[slot_idx]) {
        loggers::CAPABILITY::ERROR("槽位索引(%u, %u)未被占用", capidx::group(idx), slot_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
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

// 通过KOP分配CGroup实例
void *CGroup::operator new(size_t size) {
    assert(size == sizeof(CGroup));
    return kop::CGroup->alloc();
}

// 通过KOP删除CGroup实例
void CGroup::operator delete(void *ptr) {
    kop::CGroup->free(static_cast<CGroup *>(ptr));
}

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

Result<void> CSpace::clone(CapIdx idx, Capability *parent) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    CGroup *group = group_at(group_idx);
    return group->clone(idx, parent);
}

Result<void> CSpace::migrate(CapIdx idx, Capability *origin) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    CGroup *group = group_at(group_idx);
    return group->migrate(idx, origin);
}

Result<void> CSpace::remove(CapIdx idx) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_groups[group_idx]) {
        loggers::CAPABILITY::ERROR("CGroup索引%u在CSpace %d中未被创建", group_idx, sp_idx);
        return {unexpect, ErrCode::OUT_OF_BOUNDARY};
    }
    return _groups[group_idx]->remove(idx);
}

Result<CGroup *> CSpace::group(CapIdx idx) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_groups[group_idx]) {
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    return _groups[group_idx];
}

Result<Capability *> CSpace::get(CapIdx idx) {
    const size_t group_idx = capidx::group(idx);
    if (group_idx >= CSPACE_SIZE) {
        loggers::CAPABILITY::ERROR("CGroup索引%u超出CSpace %d容量", group_idx, sp_idx);
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
    }
    if (!_groups[group_idx]) {
        loggers::CAPABILITY::ERROR("CGroup索引%u在CSpace %d中未被创建", group_idx, sp_idx);
        return {unexpect,ErrCode::OUT_OF_BOUNDARY};
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