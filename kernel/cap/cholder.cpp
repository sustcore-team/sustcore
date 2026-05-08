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
#include <env.h>
#include <perm/perm.h>
#include <sustcore/capability.h>
#include <task/task_struct.h>

namespace key {
    struct cholder : public env::key::chman {};
}  // namespace key

namespace cap {
    CHolder::CHolder(size_t id) : _space(), _id(id) {}

    CHolder::~CHolder() {}

    Result<CHolder *> CHolder::current() {
        auto *scheduler = env::inst().scheduler();
        if (scheduler == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto *tcb = scheduler->current_tcb();
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
                                          PermissionBits &&perm) {
        if (!cap::valid(idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (payload == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (payload->type_id() != perm.type) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (_space.get(idx) != nullptr) {
            unexpect_return(ErrCode::SLOT_BUSY);
        }

        auto *cap = new Capability(payload, std::move(perm));
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
        if (!src_cap->perm().basic_imply(perm::basic::CLONE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto *cloned_cap = src_cap->clone();
        auto set_res = set_slot(target_idx, cloned_cap);
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
        if (!src_cap->perm().basic_imply(perm::basic::MIGRATE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto move_res = _space.move(target_idx, src_idx);
        propagate(move_res);
        void_return();
    }

    Result<void> CHolder::internal_derive(CapIdx target_idx, CapIdx src_idx,
                                          const PermissionBits &new_perm) {
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

    Result<ReceiveToken> CHolder::internal_send(CapIdx src_idx,
                                                size_t target_id) {
        // make sure it's a valid capability index
        auto access_res = internal_lookup(src_idx);
        propagate(access_res);

        Capability *src_cap = access_res.value();
        if (!src_cap->perm().basic_imply(perm::basic::MIGRATE)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto chman = env::inst().chman(key::cholder());
        auto target_holder_res = chman->get_holder(target_id);
        propagate(target_holder_res);

        size_t timestamp = chman->timestamp();

        // add a send record to the queue
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        _send_records[_send_record_idx] = SendRecord(
            timestamp, {.sender_id = _id, .cap_idx = src_idx}, target_id);
        const size_t record_idx = _send_record_idx;
        _send_record_idx        = (_send_record_idx + 1) % SENDRECORDS;

        return ReceiveToken{
            .sender_id = _id, .record_idx = record_idx, .timestamp = timestamp};
    }

    Result<CapIdx> CHolder::lookup_record(size_t receiver_id,
                                          ReceiveToken token) {
        // 测试token本身是否合法
        if (token.sender_id != _id) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }
        if (token.record_idx >= SENDRECORDS) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }

        // 从发送记录中获取对应的记录
        const SendRecord &record = _send_records[token.record_idx];

        // 测试记录是否合法
        if (record.timestamp() != token.timestamp) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }
        if (record.sender_id() != token.sender_id) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }
        if (record.target_id() != receiver_id) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }
        // 还需要try access
        auto access_res = internal_lookup(record.cap_idx());
        if (!access_res.has_value()) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }

        // token有效, timestamp与sender_id都匹配, 记录中的cap_idx也合法
        return record.cap_idx();
    }

    Result<void> CHolder::remove_record(size_t receiver_id,
                                        ReceiveToken token) {
        if (token.sender_id != _id) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }
        if (token.record_idx >= SENDRECORDS) {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }

        const SendRecord &record = _send_records[token.record_idx];
        if (record.timestamp() != token.timestamp ||
            record.sender_id() != token.sender_id ||
            record.target_id() != receiver_id)
        {
            unexpect_return(ErrCode::INVALID_TOKEN);
        }

        const size_t record_idx = token.record_idx;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        _send_records[record_idx] = SendRecord();
        void_return();
    }

    Result<void> CHolder::internal_recv(CapIdx target_idx, ReceiveToken token) {
        if (!cap::valid(target_idx)) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (_space.get(target_idx) != nullptr) {
            unexpect_return(ErrCode::SLOT_BUSY);
        }

        auto *chman = env::inst().chman(key::cholder());
        auto sender_holder_res = chman->get_holder(token.sender_id);
        propagate(sender_holder_res);

        CHolder *sender = sender_holder_res.value();
        auto record_res = sender->lookup_record(_id, token);
        propagate(record_res);
        CapIdx src_idx = record_res.value();

        auto moved_cap_res = sender->take_slot(src_idx);
        propagate(moved_cap_res);

        Capability *moved_cap = moved_cap_res.value();
        auto set_res = set_slot(target_idx, moved_cap);
        if (!set_res.has_value()) {
            auto rollback_res = sender->set_slot(src_idx, moved_cap);
            assert(rollback_res.has_value());
            propagate_return(set_res);
        }

        // 移除发送记录
        auto remove_res = sender->remove_record(_id, token);
        propagate(remove_res);

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
                                 const PermissionBits &new_perm) {
        return current().and_then([&](CHolder *holder) {
            return holder->internal_derive(target_idx, src_idx, new_perm);
        });
    }

    Result<ReceiveToken> CHolder::send(CapIdx src_idx, size_t target_id) {
        return current().and_then([=](CHolder *holder) {
            return holder->internal_send(src_idx, target_id);
        });
    }

    Result<void> CHolder::recv(CapIdx target_idx, ReceiveToken token) {
        return current().and_then([=](CHolder *holder) {
            return holder->internal_recv(target_idx, token);
        });
    }
}  // namespace cap
