/**
 * @file entity.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 与 TCB 布局无关的调度实体、策略存储和统计数据。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <cpu/local.h>
#include <tay/bits.h>
#include <tay/list.h>
#include <tay/units.h>
#include <tay/variant.h>

namespace scheduler {
    class RunQueue;
    struct ThreadSchedOps;
    using cpu::CpuId;

    inline constexpr auto RR_TIME_SLICE = 10_ms;

    enum class ClassType : u8_t {
        IDLE,
        FAIR,
        RR,
        RT,
    };

    enum class QueueState : u8_t {
        DETACHED,
        QUEUED,
        CURRENT,
        MIGRATING,
    };

    class OwnerToken final {
    public:
        constexpr OwnerToken() noexcept = default;

    private:
        explicit constexpr OwnerToken(void *pointer) noexcept : pointer_(pointer) {}

        void *pointer_ = nullptr;

        friend struct ThreadSchedOps;
    };

    struct SchedEntity;
    using SchedQueueHook = tay::intrusive_list_hook<SchedEntity *, SchedEntity *>;

    struct SchedEntity final {
        ClassType class_type   = ClassType::RR;
        QueueState queue_state = QueueState::DETACHED;
        CpuId cpu{};
        RunQueue *run_queue = nullptr;
        SchedQueueHook queue_hook{};

    private:
        OwnerToken owner_{};

        friend struct ThreadSchedOps;
    };

    struct SchedStatistics final {
        units::duration runtime{};
        units::duration ready_wait{};
        units::time last_start{};
        units::time last_enqueue{};
        u64_t voluntary_switches   = 0;
        u64_t involuntary_switches = 0;
        u64_t migrations           = 0;
    };

    struct IdleEntity final {};

    struct RrEntity final {
        units::duration remaining_slice = RR_TIME_SLICE;
    };

    struct FairEntity final {
        u64_t vruntime = 0;
        u64_t deadline = 0;
        u32_t weight   = 0;
    };

    struct RtEntity final {
        units::duration budget{};
        units::duration period{};
        units::time deadline{};
    };

    using PolicyStorage = tay::variant<IdleEntity, FairEntity, RrEntity, RtEntity>;

    struct SchedStorage final {
        SchedEntity entity{};
        SchedStatistics statistics{};
        PolicyStorage policy{RrEntity{}};
    };

    inline void initialize_policy(SchedStorage &storage, ClassType class_type) noexcept {
        storage.entity.class_type = class_type;
        switch (class_type) {
            case ClassType::IDLE: storage.policy = PolicyStorage{IdleEntity{}}; break;
            case ClassType::FAIR: storage.policy = PolicyStorage{FairEntity{}}; break;
            case ClassType::RR:
                storage.policy = PolicyStorage{RrEntity{.remaining_slice = RR_TIME_SLICE}};
                break;
            case ClassType::RT: storage.policy = PolicyStorage{RtEntity{}}; break;
        }
    }
}  // namespace scheduler
