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
#include <cap/cspace.h>
#include <cassert>
#include <kio.h>
#include <perm/permission.h>
#include <sus/defer.h>
#include <sus/id.h>
#include <sus/list.h>
#include <sus/queue.h>
#include <sustcore/capability.h>

// capability

Capability::Capability(Payload *payload, PermissionBits &&perm, CSpace *space,
                       CapIdx idx)
    : TreeBase(),
      _payload(payload),
      _is_root(true),
      _perm(std::move(perm)),
      _space(space),
      _idx(idx) {}

Capability::Capability(Capability *parent, PermissionBits &&perm, CSpace *space,
                       CapIdx idx)
    : TreeBase(parent),
      _payload(parent->_payload),
      _is_root(false),
      _perm(std::move(perm)),
      _space(space),
      _idx(idx) {}

Capability::Capability(Capability *origin, CSpace *space, CapIdx idx)
    : TreeBase(origin->parent, std::move(origin->children)),
      _payload(origin->_payload),
      _is_root(origin->_is_root),
      // 注意: origin的权限位被移动到新Capability中, 因此origin的权限位已被清空
      _perm(std::move(origin->_perm)),
      _space(space),
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
CapErrCode Capability::kill(Capability *cap) {
    assert(cap != nullptr);
    // 定位到cap所在的CSpace与Index
    CSpace *sp = cap->_space;
    assert(sp != nullptr);
    CapIdx idx = cap->_idx;
    assert(!idx.nullable());
    cap->murder_flag = true;
    return sp->remove(idx);
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
    CAPABILITY::DEBUG("开始移除(%d, %d)@Space %d", this->_idx.group,
                      this->_idx.slot, this->_space->sp_idx);
    // 通知父节点移除自己
    if (parent != nullptr) {
        CAPABILITY::DEBUG("从(%d, %d)@Space %d中移除自身", parent->_idx.group,
                          parent->_idx.slot, parent->_space->sp_idx);
        parent->remove_child(this);
    }

    // 删除所有的子能力
    __working_queue.clear();
    for (auto &subcap : children) {
        if (! __working_queue.push(subcap)) {
            CAPABILITY::FATAL("工作队列已满!无法删除子能力!");
            panic("能力删除时出现故障!");
        }
    }

    // 通过广度优先搜索删除其子能力
    while (! __working_queue.empty()) {
        // 取出头部的能力
        Capability *cur = __working_queue.front();
        __working_queue.pop();

        // 加入该能力的直接子能力
        for (auto &subcap : cur->children) {
            if (! __working_queue.push(subcap)) {
                CAPABILITY::FATAL("工作队列已满!无法删除子能力!");
                panic("能力删除时出现故障!");
            }
            CAPABILITY::DEBUG("加入工作队列: (%d, %d)@Space %d", subcap->_idx.group,
                              subcap->_idx.slot, subcap->_space->sp_idx);
        }

        // 删除该能力
        CAPABILITY::DEBUG("移除(%d, %d)@Space %d", cur->_idx.group, cur->_idx.slot,
                          cur->_space->sp_idx);
        kill(cur);
    }

    // 根部Capability持有这个Payload
    // 因此需要删除这个Payload
    if (_is_root && _payload != nullptr) {
        CAPABILITY::DEBUG("Deleting payloads");
        delete _payload;
    }
}

CapErrCode Capability::revoke(Capability *subcap) {
    if (subcap->parent != this) {
        CAPABILITY::ERROR("无法撤销非直接子能力");
        return CapErrCode::INVALID_CAPABILITY;
    }
    // 将subcap从子节点列表中移除
    return kill(subcap);
}

CHolder *Capability::holder(void) const {
    return _space->holder();
}