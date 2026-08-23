/**
 * @file class.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 无虚调用的 FIFO/RR 调度类与 per-CPU RunQueue。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <scheduler/entity.h>
#include <tay/intrusive.h>
#include <tay/list.h>
#include <tay/panic.h>
#include <tay/spinlock.h>

#include <cstddef>

namespace scheduler {
    enum class EnterReason : u8_t {
        ADMIT,
        WAKE,
        YIELD,
        PREEMPT,
        MIGRATE,
        POLICY_CHANGE,
    };

    enum class LeaveReason : u8_t {
        DISPATCH,
        SUSPEND,
        MIGRATE,
        POLICY_CHANGE,
        TERMINATE,
    };

    enum class SelectReason : u8_t {
        NO_CURRENT,
        WAKE,
        YIELD,
        TICK,
        BLOCK,
        EXIT,
        RESCHEDULE_IPI,
    };

    enum class RunQueueFlags : u32_t {
        NONE           = 0,
        NEED_RESCHED   = 1U << 0,
        DEADLINE_DIRTY = 1U << 1,
    };

    struct EnterContext final {
        EnterReason reason;
        units::time now{};
        units::duration unaccounted_runtime{};
    };

    struct LeaveContext final {
        LeaveReason reason;
        units::time now{};
    };

    struct SelectContext final {
        SelectReason reason;
        units::time now{};
        SchedEntity *current = nullptr;
        units::duration current_runtime{};
        bool force = false;
    };

    struct EnterResult final {
        bool should_preempt = false;
    };

    enum class SelectAction : u8_t {
        KEEP_CURRENT,
        SWITCH,
        USE_IDLE,
    };

    struct SelectResult final {
        SelectAction action    = SelectAction::KEEP_CURRENT;
        SchedEntity *candidate = nullptr;
    };

    using sched_entity_list = tay::intrusive_list<
        SchedEntity, tay::locate_member<SchedEntity, SchedQueueHook, &SchedEntity::queue_hook>>;

    struct RrRunQueue final {
        sched_entity_list ready{};
    };

    class RunQueue final {
    public:
        explicit constexpr RunQueue(CpuId cpu = {}) noexcept : cpu(cpu) {}

        RunQueue(const RunQueue &)            = delete;
        RunQueue &operator=(const RunQueue &) = delete;
        RunQueue(RunQueue &&)                 = delete;
        RunQueue &operator=(RunQueue &&)      = delete;

        [[nodiscard]] constexpr bool has_flag(RunQueueFlags flag) const noexcept {
            return (static_cast<u32_t>(flags) & static_cast<u32_t>(flag)) != 0;
        }

        constexpr void set_flag(RunQueueFlags flag) noexcept {
            flags =
                static_cast<RunQueueFlags>(static_cast<u32_t>(flags) | static_cast<u32_t>(flag));
        }

        constexpr void clear_flag(RunQueueFlags flag) noexcept {
            flags =
                static_cast<RunQueueFlags>(static_cast<u32_t>(flags) & ~static_cast<u32_t>(flag));
        }

        CpuId cpu{};
        tay::spinlock lock{};
        SchedEntity *current = nullptr;
        SchedEntity *idle    = nullptr;
        RrRunQueue rr{};
        size_t queued_count         = 0;
        u64_t load                  = 0;
        RunQueueFlags flags         = RunQueueFlags::NONE;
        SelectReason resched_reason = SelectReason::NO_CURRENT;
        u64_t transition_gen        = 0;
    };

    class RrClass final {
    public:
        static constexpr ClassType CLASS_TYPE = ClassType::RR;

        [[nodiscard]] EnterResult enter(RrRunQueue &run_queue, SchedEntity &entity,
                                        const EnterContext &) noexcept {
            run_queue.ready.push_back(&entity);
            return {};
        }

        void leave(RrRunQueue &run_queue, SchedEntity &entity, const LeaveContext &) noexcept {
            if (!run_queue.ready.linked(&entity))
                tay::panic("RR leave received an unlinked entity");
            (void)run_queue.ready.remove(&entity);
        }

        [[nodiscard]] SelectResult select(RrRunQueue &run_queue,
                                          const SelectContext &context) noexcept {
            auto *candidate = run_queue.ready.empty() ? nullptr : run_queue.ready.front();
            if (candidate == nullptr)
                return SelectResult{.action = context.current == nullptr
                                                  ? SelectAction::USE_IDLE
                                                  : SelectAction::KEEP_CURRENT};
            if (context.current == nullptr || context.force || context.reason == SelectReason::TICK)
                return SelectResult{.action = SelectAction::SWITCH, .candidate = candidate};
            return {};
        }
    };

    class IdleClass final {
    public:
        static constexpr ClassType CLASS_TYPE = ClassType::IDLE;

        [[nodiscard]] SelectResult select(const RunQueue &run_queue,
                                          const SelectContext &context) const noexcept {
            if (context.current != nullptr)
                return {};
            if (run_queue.idle == nullptr)
                return SelectResult{.action = SelectAction::USE_IDLE};
            return SelectResult{.action = SelectAction::USE_IDLE, .candidate = run_queue.idle};
        }
    };

    class SchedClassSet final {
    public:
        [[nodiscard]] EnterResult enter(RunQueue &run_queue, SchedEntity &entity,
                                        const EnterContext &context) noexcept {
            const auto expected = context.reason == EnterReason::MIGRATE ? QueueState::MIGRATING
                                                                         : QueueState::DETACHED;
            if (entity.queue_state != expected || entity.run_queue != nullptr)
                tay::panic("scheduler enter received an entity in an invalid queue state");
            if (entity.class_type != ClassType::RR)
                tay::panic("only RR entities may enter the ready queue in scheduler phase S3");

            auto result        = rr_.enter(run_queue.rr, entity, context);
            entity.queue_state = QueueState::QUEUED;
            entity.cpu         = run_queue.cpu;
            entity.run_queue   = &run_queue;
            ++run_queue.queued_count;
            run_queue.load = run_queue.queued_count;
            ++run_queue.transition_gen;
            if (run_queue.current == nullptr || run_queue.current->class_type == ClassType::IDLE)
                result.should_preempt = true;
            return result;
        }

        void leave(RunQueue &run_queue, SchedEntity &entity, const LeaveContext &context) noexcept {
            if (entity.queue_state != QueueState::QUEUED || entity.run_queue != &run_queue ||
                run_queue.queued_count == 0)
                tay::panic("scheduler leave received an entity outside the run queue");
            if (entity.class_type != ClassType::RR)
                tay::panic("only RR entities may leave the ready queue in scheduler phase S3");

            rr_.leave(run_queue.rr, entity, context);
            --run_queue.queued_count;
            run_queue.load     = run_queue.queued_count;
            entity.queue_state = context.reason == LeaveReason::MIGRATE ? QueueState::MIGRATING
                                                                        : QueueState::DETACHED;
            entity.run_queue   = nullptr;
            ++run_queue.transition_gen;
        }

        [[nodiscard]] SelectResult select(RunQueue &run_queue,
                                          const SelectContext &context) noexcept {
            auto selected = rr_.select(run_queue.rr, context);
            if (selected.action != SelectAction::USE_IDLE)
                return selected;
            return idle_.select(run_queue, context);
        }

    private:
        RrClass rr_{};
        IdleClass idle_{};
    };
}  // namespace scheduler
