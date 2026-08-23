/**
 * @file adapter.h
 * @brief 在 SchedCore 边界恢复 SchedEntity 所属 Thread。
 */

#pragma once

#include <obj/thread.h>
#include <scheduler/entity.h>

namespace scheduler {
    struct ThreadSchedOps final {
        using Owner = task::Thread;

        static void initialize(Owner &thread, ClassType class_type = ClassType::RR) noexcept {
            auto &storage         = thread.scheduler_storage();
            storage.entity.owner_ = OwnerToken{&thread};
            initialize_policy(storage, class_type);
        }

        [[nodiscard]] static SchedEntity &entity(Owner &thread) noexcept {
            return thread.scheduler_storage().entity;
        }

        [[nodiscard]] static const SchedEntity &entity(const Owner &thread) noexcept {
            return thread.scheduler_storage().entity;
        }

        [[nodiscard]] static Owner &owner(SchedEntity &entity) noexcept {
            return *static_cast<Owner *>(entity.owner_.pointer_);
        }

        [[nodiscard]] static const Owner &owner(const SchedEntity &entity) noexcept {
            return *static_cast<const Owner *>(entity.owner_.pointer_);
        }
    };
}  // namespace scheduler
