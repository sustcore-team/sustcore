/**
 * @file milestones.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核初始化阶段模型与推进接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/bits.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/format.h>

namespace init {
    enum class Milestone : u8_t {
        RESET,
        EARLT_CPPRT,
        MEMORY_READY,
        HEAP_READY,
        GLOBAL_CTORS_READY,
        VIRTUAL_MEMORY_READY,
        FIRMWARE_READY,
        CPU_TOPOLOGY_READY,
        TRAPS_READY,
        IRQ_READY,
        TIMER_READY,
        SCHEDULER_READY,
        SMP_READY,
        RUNNING,
    };

    inline const char* to_string(Milestone milestone) noexcept {
        switch (milestone) {
            case Milestone::RESET:                return "重置";
            case Milestone::EARLT_CPPRT:          return "早期 C++ 运行时就绪";
            case Milestone::MEMORY_READY:         return "内存就绪";
            case Milestone::HEAP_READY:           return "堆就绪";
            case Milestone::GLOBAL_CTORS_READY:   return "C++ 全局构造就绪";
            case Milestone::VIRTUAL_MEMORY_READY: return "最终虚拟内存就绪";
            case Milestone::FIRMWARE_READY:       return "固件就绪";
            case Milestone::CPU_TOPOLOGY_READY:   return "CPU 拓扑就绪";
            case Milestone::TRAPS_READY:          return "陷阱处理就绪";
            case Milestone::IRQ_READY:            return "中断请求处理就绪";
            case Milestone::TIMER_READY:          return "定时器就绪";
            case Milestone::SCHEDULER_READY:      return "调度器就绪";
            case Milestone::SMP_READY:            return "SMP 就绪";
            case Milestone::RUNNING:              return "运行中";
        }
        return "<未知初始化里程碑>";
    }

    [[nodiscard]] Milestone milestone() noexcept;
    [[nodiscard]] constexpr tay::expected<Milestone, tay::error_code> next_milestone(
        Milestone current) noexcept {
        switch (current) {
            case Milestone::RESET:                return Milestone::EARLT_CPPRT;
            case Milestone::EARLT_CPPRT:          return Milestone::MEMORY_READY;
            case Milestone::MEMORY_READY:         return Milestone::HEAP_READY;
            case Milestone::HEAP_READY:           return Milestone::GLOBAL_CTORS_READY;
            case Milestone::GLOBAL_CTORS_READY:   return Milestone::VIRTUAL_MEMORY_READY;
            case Milestone::VIRTUAL_MEMORY_READY: return Milestone::FIRMWARE_READY;
            case Milestone::FIRMWARE_READY:       return Milestone::CPU_TOPOLOGY_READY;
            case Milestone::CPU_TOPOLOGY_READY:   return Milestone::TRAPS_READY;
            case Milestone::TRAPS_READY:          return Milestone::IRQ_READY;
            case Milestone::IRQ_READY:            return Milestone::TIMER_READY;
            case Milestone::TIMER_READY:          return Milestone::SCHEDULER_READY;
            case Milestone::SCHEDULER_READY:      return Milestone::SMP_READY;
            case Milestone::SMP_READY:            return Milestone::RUNNING;
            case Milestone::RUNNING:              break;
        }
        return tay::Err(tay::error_code::OUT_OF_RANGE);
    }
    static_assert(*next_milestone(Milestone::MEMORY_READY) == Milestone::HEAP_READY);
    static_assert(*next_milestone(Milestone::HEAP_READY) == Milestone::GLOBAL_CTORS_READY);
    static_assert(*next_milestone(Milestone::GLOBAL_CTORS_READY) ==
                  Milestone::VIRTUAL_MEMORY_READY);
    static_assert(*next_milestone(Milestone::SMP_READY) == Milestone::RUNNING);
    static_assert(!next_milestone(Milestone::RUNNING).has_value());
    void advance(Milestone expected, Milestone next) noexcept;
}  // namespace init

namespace tay {
    template <>
    struct formatter<init::Milestone> {
        constexpr format_parse_context::iterator parse(format_parse_context& context) noexcept {
            return context.begin();
        }

        template <class FormatContext>
        typename FormatContext::iterator format(const init::Milestone& milestone,
                                                FormatContext& context) const {
            context.write(init::to_string(milestone));
            return context.out();
        }
    };
}  // namespace tay
