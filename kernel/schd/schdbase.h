/**
 * @file schdbase.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief scheduler base
 * @version alpha-1.0.0
 * @date 2026-03-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>
#include <sus/nonnull.h>
#include <sus/types.h>
#include <sustcore/errcode.h>

#include <concepts>

enum class ThreadState { EMPTY = 0, READY = 1, RUNNING = 2, YIELD = 3 };

constexpr const char *to_string(ThreadState state) {
    switch (state) {
        case ThreadState::EMPTY:   return "EMPTY";
        case ThreadState::READY:   return "READY";
        case ThreadState::RUNNING: return "RUNNING";
        case ThreadState::YIELD:   return "YIELD";
        default:                   return "UNKNOWN";
    }
}

namespace schd {

    enum class ClassType { RR, FCFS };

    constexpr const char *to_string(ClassType type) {
        switch (type) {
            case ClassType::RR:   return "RR";
            case ClassType::FCFS: return "FCFS";
        }
        return "UNKNOWN";
    }

    struct SchedMeta {
        ThreadState state;
        util::ListHead<SchedMeta> rq_head;
        b64 flags;

        constexpr static b64 FLAGS_NEED_RESCHED = 0x1;

        template <b64 set_flags>
        constexpr void flags_set() {
            flags |= set_flags;
        }

        template <b64 reset_flags>
        constexpr void flags_reset() {
            flags &= ~reset_flags;
        }

        template <typename SUType>
        constexpr static size_t ENTITY_OFFSET = offsetof(SUType, basic_entity);

        template <typename SUType>
        inline static util::nonnull<SchedMeta *> as_entity(
            util::nonnull<SUType *> unit) {
            return util::guarantee_nonnull(&unit->basic_entity);
        }

        template <typename SUType>
        inline static util::nonnull<SUType *> asunit(
            util::nonnull<SchedMeta *> entity) {
            auto *entity_ptr = reinterpret_cast<char *>(entity.get());
            auto *su_ptr     = entity_ptr - ENTITY_OFFSET<SUType>;
            return util::guarantee_nonnull(reinterpret_cast<SUType *>(su_ptr));
        }
    };

    struct RQ {
        util::IntrusiveList<SchedMeta, &SchedMeta::rq_head> fcfs_list;
        util::IntrusiveList<SchedMeta, &SchedMeta::rq_head> rr_list;
    };

    template <template <typename> class SchdPolicy, typename SU>
    concept SchdClassBasicTrait =
        requires(SchdPolicy<SU> &policy, util::nonnull<RQ *> rq,
                 util::nonnull<SU *> unit) {
            {
                SchdPolicy<SU>::CLASS_TYPE
            } -> std::convertible_to<ClassType>;
            {
                policy.current()
            } -> std::same_as<Result<util::nonnull<SU *>>>;
            {
                policy.enqueue(rq, unit)
            } -> std::same_as<Result<void>>;
            {
                policy.dequeue(rq, unit)
            } -> std::same_as<Result<void>>;
            {
                policy.pick_next(rq)
            } -> std::same_as<Result<util::nonnull<SU *>>>;
            {
                policy.put_prev(rq, unit)
            } -> std::same_as<Result<void>>;
            {
                policy.yield(rq)
            } -> std::same_as<Result<void>>;
            {
                policy.on_tick(rq, unit)
            } -> std::same_as<Result<void>>;
        };

    template <template <typename> class SchdPolicy, typename SU>
    concept SchdClassTrait = SchdClassBasicTrait<SchdPolicy, SU>;
}  // namespace schd