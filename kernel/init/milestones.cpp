/**
 * @file milestones.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 全局内核初始化进度发布
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <init/milestones.h>
#include <log.h>

namespace init {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    constinit std::atomic<Milestone> __current_milestone{Milestone::RESET};

    Milestone milestone() noexcept {
        return __current_milestone.load(std::memory_order_acquire);
    }

    void advance(Milestone expected, Milestone next) noexcept {
        const auto successor = next_milestone(expected);
        if (!successor || *successor != next) {
            kernel::log::panic("无效的初始化里程碑边");
        }
        // release 将本阶段完成的初始化统一发布；CAS 同时拒绝跳级或重复推进。
        if (!__current_milestone.compare_exchange_strong(expected, next, std::memory_order_release,
                                                         std::memory_order_acquire))
        {
            kernel::log::panic("无效的初始化里程碑顺序");
        }

        kernel::log::info("初始化里程碑: {} -> {}", expected, next);
    }
}  // namespace init
