/**
 * @file cholder.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 能力持有者
 * @version alpha-1.0.0
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cap/capability.h>
#include <sus/map.h>
#include <sus/queue.h>
#include <sus/raii.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>

#include <utility>

namespace cap {
    // 能够持有能力的对象称为能力持有者 (Capability Holder)
    // 例如, Process就是一种典型的能力持有者.
    // 又如, VFS也是一种能力持有者, 因为它持有着文件系统的能力 (如根目录的能力).
    class CHolder {
    private:
        CSpace _space;
        size_t _id;
    public:
        CHolder(size_t _id);
        ~CHolder();

        [[nodiscard]]
        constexpr size_t id() const {
            return _id;
        }

        [[nodiscard]]
        constexpr const CSpace &space() const {
            return _space;
        }
        [[nodiscard]]
        constexpr CSpace &space() {
            return _space;
        }

        [[nodiscard]]
        Result<Capability *> access(CapIdx idx) {
            return internal_lookup(idx);
        }

        [[nodiscard]]
        Result<CapIdx> internal_lookup_freeslot() {
            return _space.lookup_freeslot();
        }

        [[nodiscard]]
        Result<Capability *> internal_lookup(CapIdx idx);

        [[nodiscard]]
        Result<void> internal_insert(CapIdx idx, Payload *payload) {
            assert(payload != nullptr);
            return internal_insert(idx, payload, perm::allperm());
        }

        [[nodiscard]]
        Result<void> internal_insert(CapIdx idx, Payload *payload, b64 perm);

        template <typename PayloadType, typename... Args>
        [[nodiscard]]
        Result<void> internal_create(CapIdx idx, Args &&...args) {
            if (!cap::valid(idx)) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            auto *payload = new PayloadType(std::forward<Args>(args)...);
            if (payload == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            auto insert_res = internal_insert(idx, payload);
            if (!insert_res.has_value()) {
                delete payload;
                propagate_return(insert_res);
            }
            void_return();
        }

        [[nodiscard]]
        Result<void> internal_remove(CapIdx idx);

        void internal_clear();

        [[nodiscard]]
        Result<void> internal_clone(CapIdx target_idx, CapIdx src_idx);

        [[nodiscard]]
        Result<void> internal_migrate(CapIdx target_idx, CapIdx src_idx);

        [[nodiscard]]
        Result<void> internal_derive(CapIdx target_idx, CapIdx src_idx,
                                     b64 new_perm);

        [[nodiscard]]
        Result<void> internal_downgrade(CapIdx idx, b64 new_perm);

        [[nodiscard]]
        Result<void> internal_copy_all_to(CHolder &dst) const;

        template <typename PayloadType, typename... Args>
        [[nodiscard]]
        static Result<void> create(CapIdx idx, Args &&...args) {
            return current().and_then([&](CHolder *holder) {
                return holder->internal_create<PayloadType>(
                    idx, std::forward<Args>(args)...);
            });
        }

        [[nodiscard]]
        static Result<CapIdx> get_free_slot();

        [[nodiscard]]
        static Result<Capability *> lookup(CapIdx idx);

        [[nodiscard]]
        static Result<void> remove(CapIdx idx);

        [[nodiscard]]
        static Result<void> clone(CapIdx target_idx, CapIdx src_idx);

        [[nodiscard]]
        static Result<void> migrate(CapIdx target_idx, CapIdx src_idx);

        [[nodiscard]]
        static Result<void> derive(CapIdx target_idx, CapIdx src_idx,
                                   b64 new_perm);

        [[nodiscard]]
        static Result<void> downgrade(CapIdx idx, b64 new_perm);

        [[nodiscard]]
        static Result<CHolder *> current();

    private:
        [[nodiscard]]
        Result<void> set_slot(CapIdx idx, Capability *cap);

        [[nodiscard]]
        Result<Capability *> take_slot(CapIdx idx);
    };

    class CHolderManager {
    private:
        size_t __cur_id = 0;
        constexpr size_t _new_id() {
            return __cur_id++;
        }
        util::LinkedMap<size_t, CHolder *> _holders;
        size_t _timestamp = 1;  // 用于记录发送记录的时间戳, 每次发送能力时递增
    public:
        static void init();
        static CHolderManager &inst();

        CHolderManager() = default;

        [[nodiscard]]
        Result<CHolder *> get_holder(size_t _id) const {
            return _holders.get(_id)
                .transform_error(always(ErrCode::OUT_OF_BOUNDARY))
                .transform(
                    std::mem_fn(&std::reference_wrapper<CHolder *const>::get));
        }

        template <typename... Args>
        Result<CHolder *> create_holder(Args &&...args) {
            size_t _id  = _new_id();
            auto holder = new CHolder(_id, std::forward<Args>(args)...);
            _holders.put(_id, holder);
            return holder;
        }

        Result<void> remove_holder(size_t _id) {
            if (!_holders.contains(_id)) {
                return {unexpect, ErrCode::OUT_OF_BOUNDARY};
            }
            auto holder = _holders.get(_id).value();
            delete holder;
            _holders.remove(_id);
            void_return();
        }

        [[nodiscard]]
        size_t timestamp() {
            return _timestamp++;
        }
    };
}  // namespace cap
