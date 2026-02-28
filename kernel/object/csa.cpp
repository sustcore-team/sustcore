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