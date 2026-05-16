/**
 * @file cholder.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力持有者
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <perm/perm.h>
#include <sustcore/capability.h>
#include <task/task_struct.h>
#include <task/scheduler.h>

namespace {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static cap::CHolderManager inst_cholder_manager;
}  // namespace

namespace cap {
    void CHolderManager::init() {
        // call the constructor explicitly to ensure the instance is initialized before use
        new (&inst_cholder_manager) CHolderManager();
    }

    CHolderManager &CHolderManager::inst() {
        return inst_cholder_manager;
    }

    CHolder::CHolder(size_t id) : _space(), _id(id) {}

    CHolder::~CHolder() {}

    Result<CHolder *> CHolder::current() {
        auto *tcb = schd::Scheduler::inst().current_tcb();
        if (tcb == nullptr || tcb->task == nullptr ||
            tcb->task->cholder == nullptr)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        return tcb->task->cholder;
    }

    Result<Capability *> CHolder::internal_lookup(CapIdx idx) {
        if (!cap::valid(idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        Capability *cap = _space.get(idx);
        if (cap == nullptr) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        return cap;
    }

    Result<void> CHolder::set_slot(CapIdx idx, Capability *cap) {
        if (!cap::valid(idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        return _space.set(idx, cap);
    }

    Result<Capability *> CHolder::take_slot(CapIdx idx) {
        if (!cap::valid(idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }

        Capability *cap = _space.take(idx);
        if (cap == nullptr) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        return cap;
    }

    Result<void> CHolder::internal_insert(CapIdx idx, Payload *payload,
                                          b64 permissions) {
        if (!cap::valid(idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (payload == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (_space.get(idx) != nullptr) {
            unexpect_return(ErrCode::SLOT_BUSY);
        }

        auto *cap = new Capability(payload, permissions);
        if (cap == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        auto set_res = set_slot(idx, cap);
        assert(set_res.has_value());
        return set_res;
    }

    Result<void> CHolder::internal_remove(CapIdx idx) {
        auto cap_res = internal_lookup(idx);
        propagate(cap_res);
        return set_slot(idx, nullptr);
    }

    void CHolder::internal_clear() {
        _space.clear();
    }

    Result<void> CHolder::internal_clone(CapIdx target_idx, CapIdx src_idx) {
        if (!cap::valid(target_idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (_space.get(target_idx) != nullptr) {
            unexpect_return(ErrCode::SLOT_BUSY);
        }
        auto cap_res = internal_lookup(src_idx);
        propagate(cap_res);

        Capability *src_cap = cap_res.value();
        if (!perm::imply(src_cap->perm(), perm::basic::CLONE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto *cloned_cap = src_cap->clone();
        auto set_res     = set_slot(target_idx, cloned_cap);
        if (!set_res.has_value()) {
            delete cloned_cap;
            propagate_return(set_res);
        }
        void_return();
    }

    Result<void> CHolder::internal_migrate(CapIdx target_idx, CapIdx src_idx) {
        if (!cap::valid(target_idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (target_idx != src_idx && _space.get(target_idx) != nullptr) {
            unexpect_return(ErrCode::SLOT_BUSY);
        }
        auto cap_res = internal_lookup(src_idx);
        propagate(cap_res);

        Capability *src_cap = cap_res.value();
        if (!perm::imply(src_cap->perm(), perm::basic::MIGRATE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto move_res = _space.move(target_idx, src_idx);
        propagate(move_res);
        void_return();
    }

    Result<void> CHolder::internal_derive(CapIdx target_idx, CapIdx src_idx,
                                          b64 new_perm) {
        auto clone_res = internal_clone(target_idx, src_idx);
        propagate(clone_res);

        util::Guard clone_guard([this, target_idx] {
            auto remove_res = internal_remove(target_idx);
            assert(remove_res.has_value());
        });

        auto cap_res = internal_lookup(target_idx);
        assert(cap_res.has_value());
        auto downgrade_res = cap_res.value()->downgrade(new_perm);
        propagate(downgrade_res);

        clone_guard.release();
        void_return();
    }

    Result<void> CHolder::internal_downgrade(CapIdx idx, b64 new_perm) {
        auto cap_res = internal_lookup(idx);
        propagate(cap_res);

        return cap_res.value()->downgrade(new_perm);
    }

    Result<void> CHolder::internal_copy_all_to(CHolder &dst) const {
        ErrCode err = ErrCode::SUCCESS;
        _space.foreach([&](CapIdx idx, Capability *cap) {
            if (err != ErrCode::SUCCESS) {
                return;
            }
            auto insert_res = dst.internal_insert(idx, cap->payload(), cap->perm());
            if (!insert_res.has_value()) {
                err = insert_res.error();
            }
        });
        if (err != ErrCode::SUCCESS) {
            unexpect_return(err);
        }
        void_return();
    }

    Result<CapIdx> CHolder::get_free_slot() {
        return current().and_then(
            [](CHolder *holder) { return holder->internal_lookup_freeslot(); });
    }

    Result<Capability *> CHolder::lookup(CapIdx idx) {
        return current().and_then(
            [idx](CHolder *holder) { return holder->internal_lookup(idx); });
    }

    Result<void> CHolder::remove(CapIdx idx) {
        return current().and_then(
            [idx](CHolder *holder) { return holder->internal_remove(idx); });
    }

    Result<void> CHolder::clone(CapIdx target_idx, CapIdx src_idx) {
        return current().and_then([=](CHolder *holder) {
            return holder->internal_clone(target_idx, src_idx);
        });
    }

    Result<void> CHolder::migrate(CapIdx target_idx, CapIdx src_idx) {
        return current().and_then([=](CHolder *holder) {
            return holder->internal_migrate(target_idx, src_idx);
        });
    }

    Result<void> CHolder::derive(CapIdx target_idx, CapIdx src_idx,
                                 b64 new_perm) {
        return current().and_then([&](CHolder *holder) {
            return holder->internal_derive(target_idx, src_idx, new_perm);
        });
    }

    Result<void> CHolder::downgrade(CapIdx idx, b64 new_perm) {
        return current().and_then([=](CHolder *holder) {
            return holder->internal_downgrade(idx, new_perm);
        });
    }
}  // namespace cap
