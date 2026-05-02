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

enum class ThreadState {
    EMPTY          = 0,
    INITIALIZATION = 1,
    READY          = 2,
    RUNNING        = 3,
    YIELD          = 4
};

constexpr const char *to_string(ThreadState state) {
    switch (state) {
        case ThreadState::EMPTY:          return "EMPTY";
        case ThreadState::INITIALIZATION: return "INITIALIZATION";
        case ThreadState::READY:          return "READY";
        case ThreadState::RUNNING:        return "RUNNING";
        case ThreadState::YIELD:          return "YIELD";
        default:                          return "UNKNOWN";
    }
}

namespace schd {
    // BOT is the lowest priority, served as the minimum of the class type
    // however, there is no actual BOT class, it's just a placeholder for the
    // end of the class type range
    enum class ClassType { RR, FCFS, IDLE, BOT };

    constexpr const char *to_cstring(ClassType type) {
        switch (type) {
            case ClassType::RR:   return "RR";
            case ClassType::FCFS: return "FCFS";
            case ClassType::IDLE: return "IDLE";
            case ClassType::BOT:  return "BOT";
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

        template <b64 check_flags>
        [[nodiscard]]
        constexpr bool flags_check() const {
            return (flags & check_flags) == check_flags;
        }

        template <typename SUType>
        constexpr static size_t ENTITY_OFFSET = offsetof(SUType, basic_entity);

        template <typename SUType>
        inline static util::nonnull<SchedMeta *> as_entity(
            util::nonnull<SUType *> unit) {
            return util::nnullforce(&unit->basic_entity);
        }

        template <typename SUType>
        inline static util::nonnull<SUType *> asunit(
            util::nonnull<SchedMeta *> entity) {
            auto *entity_ptr = reinterpret_cast<char *>(entity.get());
            auto *su_ptr     = entity_ptr - ENTITY_OFFSET<SUType>;
            return util::nnullforce(reinterpret_cast<SUType *>(su_ptr));
        }
    };

    struct RQ {
        util::IntrusiveList<SchedMeta, &SchedMeta::rq_head> fcfs_list;
        util::IntrusiveList<SchedMeta, &SchedMeta::rq_head> rr_list;
    };

    template <typename SU>
    class BaseSched {
    public:
        using SUType         = SU;
        virtual ~BaseSched() = default;

        SchedMeta *cursched = nullptr;

        inline util::nonnull<SchedMeta *> asmeta(util::nonnull<SUType *> unit) {
            return SchedMeta::as_entity(unit);
        }

        inline util::nonnull<SUType *> asunit(util::nonnull<SchedMeta *> meta) {
            return SchedMeta::asunit<SUType>(meta);
        }

        virtual Result<util::nonnull<SUType *>> current() {
            if (cursched == nullptr) {
                unexpect_return(ErrCode::NO_RUNNABLE_THREAD);
            }
            return asunit(util::nnullforce(cursched));
        }

        /**
         * @brief 将调度单元加入就绪队列
         *
         * @param rq 调度器的就绪队列
         * @param unit 需要入队的调度单元
         */
        virtual Result<void> enqueue(util::nonnull<RQ *> rq,
                                     util::nonnull<SUType *> unit) = 0;
        /**
         * @brief 将调度单元从就绪队列中移除
         *
         * @param rq 调度器的就绪队列
         * @param unit 需要移除的调度单元
         */
        virtual Result<void> dequeue(util::nonnull<RQ *> rq,
                                     util::nonnull<SUType *> unit) = 0;
        /**
         * @brief 选择下一个要运行的调度单元
         *
         * 执行该函数时, 调度器会把 cursched 指针指向下一个要运行的调度单元
         * 将下一个要运行的调度单元的状态设置为 RUNNING, 并从就绪队列中移除它
         *
         * @param rq 调度器的就绪队列
         * @return Result<util::nonnull<SUType *>> 下一个要运行的调度单元
         */
        virtual Result<util::nonnull<SUType *>> pick_next(
            util::nonnull<RQ *> rq)                                 = 0;
        /**
         * @brief 将调度单元放回就绪队列
         *
         * @param rq 调度器的就绪队列
         * @param unit 需要放回的调度单元
         */
        virtual Result<void> put_prev(util::nonnull<RQ *> rq,
                                      util::nonnull<SUType *> unit) = 0;
        /**
         * @brief 主动让出 CPU
         *
         * 这么做并不会立即切换到下一个任务, 而是为当前任务添加一个 NEED_RESCHED
         * 标志, 让调度器在合适的时候切换到下一个任务
         *
         * @param rq 调度器的就绪队列
         */
        virtual Result<void> yield(util::nonnull<RQ *> rq)          = 0;
        /**
         * @brief 每个tick调用一次, 用于更新调度单元的状态
         *
         * @param rq 调度器的就绪队列
         * @param unit 当前正在运行的调度单元
         */
        virtual Result<void> on_tick(util::nonnull<RQ *> rq,
                                     util::nonnull<SUType *> unit)  = 0;

        /**
         * @brief 判断当前任务是否要被new_su抢占(其中new_su的class
         * type总是大于当前任务的class type)
         *
         * @param rq 调度器的就绪队列
         * @param new_su 即将要运行的调度单元
         * @return true 需要抢占当前任务
         * @return false 不需要抢占当前任务
         */
        virtual bool check_preempt_curr(util::nonnull<RQ *> rq,
                                        util::nonnull<SUType *> new_su) = 0;
    };

    template <template <typename> class SchdPolicy, typename SU>
    concept SchdClassBasicTrait =
        std::derived_from<SchdPolicy<SU>, BaseSched<SU>> &&
        requires(SchdPolicy<SU> &policy, util::nonnull<RQ *> rq,
                 util::nonnull<SU *> unit) {
            {
                SchdPolicy<SU>::CLASS_TYPE
            } -> std::convertible_to<ClassType>;
        };

    template <template <typename> class SchdPolicy, typename SU>
    concept SchdClassTrait = SchdClassBasicTrait<SchdPolicy, SU>;
}  // namespace schd