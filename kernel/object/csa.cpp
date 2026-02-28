/**
 * @file csa.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief CSpace Accessor
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <object/csa.h>
#include <perm/csa.h>
#include <cassert>

CapErrCode CSAOperation::clone(CapIdx dst_idx, CSpace *src_space,
                               CapIdx src_idx) {
    using namespace perm;
    using namespace csa;

    // 检查权限
    if (!slot_imply<SLOT_INSERT>(dst_idx)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    auto cap_opt = src_space->get(src_idx);
    if (!cap_opt.present()) {
        return CapErrCode::INVALID_INDEX;
    }

    Capability *src_cap = cap_opt.value();
    if (!src_cap->perm().basic_imply(basic::CLONE)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    assert(src_cap != nullptr);
    assert(src_cap->space() == src_space);
    assert(src_cap->idx() == src_idx);

    return _space->clone(dst_idx, cap_opt.value());
}

CapErrCode CSAOperation::migrate(CapIdx dst_idx, CSpace *src_space,
                                 CapIdx src_idx) {
    using namespace perm;
    using namespace csa;

    // 检查权限
    if (!slot_imply<SLOT_INSERT>(dst_idx)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    auto cap_opt = src_space->get(src_idx);
    if (!cap_opt.present()) {
        return CapErrCode::INVALID_INDEX;
    }

    Capability *src_cap = cap_opt.value();
    if (!src_cap->perm().basic_imply(basic::MIGRATE)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    assert(src_cap != nullptr);
    assert(src_cap->space() == src_space);
    assert(src_cap->idx() == src_idx);

    CapErrCode err = _space->migrate(dst_idx, cap_opt.value());

    if (err != CapErrCode::SUCCESS) {
        return err;
    }

    // 接下来, 我们需要从src_space中移除该能力
    err = src_space->remove(src_idx);
    if (err != CapErrCode::SUCCESS) {
        // 如果移除失败, 我们需要撤销在dst_space中的迁移操作
        CapErrCode err2 = _space->remove(dst_idx);
        if (err2 != CapErrCode::SUCCESS) {
            // 刚刚migrate成功, 但是却无法remove
            // 现在, 能力空间中既有原来的能力, 又有迁移后的能力
            // 这是一个严重的问题, 需要在此处崩溃
            CAPABILITY::FATAL("迁移失败后撤销迁移操作时发生错误: %d",
                              static_cast<int>(err2));
            panic("无法撤销迁移操作, 能力空间已处于不一致状态!");
        }
        return err;
    }

    return CapErrCode::SUCCESS;
}

CapErrCode CSAOperation::remove(CapIdx idx) {
    using namespace perm::csa;
    // 检查权限
    if (!slot_imply<SLOT_REMOVE>(idx)) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    return _space->remove(idx);
}

CapIdx CSAOperation::__get_free_slot(void) {
    using namespace perm::csa;
    // 这里我们简单地从0开始线性扫描, 寻找第一个空闲槽位
    // 实际上, 可以使用更高效的数据结构来管理空闲槽位, 以避免线性扫描的性能问题
    for (size_t groupidx = 0; groupidx < CSPACE_SIZE ; groupidx++) {
        // 首先, CSA需要持有对该groupidx的INSERT权限
        if (! __slot_imply<SLOT_INSERT>(groupidx)) {
            continue;  // 没有权限访问该CGroup, 跳过
        }
        // 判断group是否为空
        if (_space->_groups[groupidx] == nullptr) {
            return CapIdx(groupidx, 0);  // 该groupidx下的第0个槽位即为一个空闲槽位
        }
        // 否则, 继续扫描该groupidx下的槽位
        for (size_t slotidx = 0; slotidx < CGROUP_SLOTS; slotidx++) {
            if (_space->_groups[groupidx]->_slot_used[slotidx]) {
                continue;  // 该槽位已被使用, 继续扫描下一个槽
            }
            return CapIdx(groupidx, slotidx);
        }
    }
    return CapIdxNull;  // 没有空闲槽位
}

CapOptional<CapIdx> CSAOperation::alloc_slot(void) {
    using namespace perm::csa;
    // 检查权限
    if (!imply<ALLOC>()) {
        return CapErrCode::INSUFFICIENT_PERMISSIONS;
    }

    CapIdx free_slot = __get_free_slot();
    if (free_slot.nullable()) {
        return CapErrCode::SLOT_BUSY;  // 没有空闲槽位
    }
    return free_slot;
}