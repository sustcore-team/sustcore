/**
 * @file schedule.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief schedule.h
 * @version alpha-1.0.0
 * @date 2026-02-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <schd/metadata.h>

namespace schd {
    // 调度器概念
    template <typename Policy, typename TCBType>
    concept SchedulerTrait = requires(Policy *policy, TCBType *thread) {
        typename Policy::MetadataType;
        {
            policy->schedule()
        } -> std::same_as<TCBType *>;
        {
            policy->current()
        } -> std::same_as<TCBType *>;
        {
            policy->add(thread)
        } -> std::same_as<void>;
        {
            policy->yield(thread)
        } -> std::same_as<void>;
        {
            policy->exit(thread)
        } -> std::same_as<void>;
    };

    template <typename TCBType, typename Metadata>
    class BaseScheduler {
    public:
        using MetadataType = Metadata;
    protected:
        BaseScheduler() {}
        ~BaseScheduler() {}
        TCBType *upcast(MetadataType *meta) {
            return reinterpret_cast<TCBType *>(meta);
        }
        MetadataType *downcast(TCBType *tcb) {
            return reinterpret_cast<MetadataType *>(tcb);
        }
    };
}  // namespace schd