/**
 * @file clock.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 时钟
 * @version alpha-1.0.0
 * @date 2026-05-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sbi/sbi.h>
#include <sus/owner.h>
#include <sus/units.h>
#include <sustcore/epacks.h>

#include <functional>
#include <optional>
#include <vector>

namespace driver {
    /**
     * @brief 系统时钟源抽象接口.
     */
    class ClockSource {
    public:
        virtual ~ClockSource() = default;

        /**
         * @brief 获取当前时钟计数值.
         *
         * @return units::tick 当前 tick 计数
         */
        [[nodiscard]]
        virtual units::tick now() const = 0;

        /**
         * @brief 获取时钟源频率.
         *
         * @return units::frequency 时钟频率
         */
        [[nodiscard]]
        virtual units::frequency frequency() const = 0;

        /**
         * @brief 将 tick 计数换算为时间值.
         *
         * @param ticks 要换算的 tick 计数
         * @return units::time 换算后的时间值
         */
        [[nodiscard]]
        units::time to_ns(units::tick ticks) const noexcept {
            return ticks / frequency();
        }
    };

    /**
     * @brief 时钟事件回调上下文.
     */
    struct ClockEventInfo {
        units::time last;
        units::time now;
    };

    /**
     * @brief 一次时钟中断事件的时间上下文.
     */
    struct ClockEvent {
    public:
        units::time last{};
        units::time now{};
    };

    /**
     * @brief 可编程下一次中断的硬件闹钟抽象接口.
     */
    class Alarm {
    public:
        using Handler = std::function<void(ClockEvent)>;

        /**
         * @brief 构造一个绑定到指定时钟源的时钟事件设备.
         *
         * @param clksrc 事件依赖的底层时钟源
         */
        constexpr explicit Alarm(ClockSource *clksrc) noexcept
            : _clksrc(clksrc) {}
        virtual ~Alarm() = default;

        /**
         * @brief 该函数会编程硬件时钟事件设备,
         * 使其在相对于"现在"的 delta 时间后产生中断.
         *
         * @param delta 事件触发的时间间隔
         */
        virtual void set_next_event(units::time delta) = 0;
        /**
         * @brief 获得该时钟事件设备能够支持的最大时间间隔.
         *
         * @return units::time 最大时间间隔
         */
        [[nodiscard]]
        virtual units::time max_delta() const       = 0;
        /**
         * @brief 注册时钟事件到期时的回调处理函数
         *
         * @param handler 事件处理函数
         */
        virtual void set_handler(Handler &&handler) = 0;

        /**
         * @brief 获取该 Alarm 绑定的时钟源.
         *
         * @return ClockSource* 底层时钟源.
         */
        [[nodiscard]]
        constexpr ClockSource *clock_source() const noexcept {
            return _clksrc;
        }

    protected:
        ClockSource *_clksrc = nullptr;
    };

    /**
     * @brief 到期动作抽象接口.
     *
     * 每个动作自带 deadline 与 canceled 状态, 由 TimeKeeper 负责调度与跳过.
     */
    class ExpireAction {
    public:
        /**
         * @brief 使用指定到期时间构造动作.
         *
         * @param deadline 动作到期时间.
         */
        constexpr explicit ExpireAction(units::time deadline) noexcept
            : _deadline(deadline) {}

        virtual ~ExpireAction() = default;

        /**
         * @brief 执行动作.
         *
         * @param event 本次时钟中断事件信息.
         */
        virtual void expire(const ClockEvent &event) noexcept = 0;

        /**
         * @brief 获取动作到期时间.
         *
         * @return units::time 到期时间.
         */
        [[nodiscard]]
        constexpr units::time deadline() const noexcept {
            return _deadline;
        }

        /**
         * @brief 覆盖动作到期时间.
         *
         * @param deadline 新的到期时间.
         */
        constexpr void set_deadline(units::time deadline) noexcept {
            _deadline = deadline;
        }

        /**
         * @brief 标记动作取消.
         */
        constexpr void cancel() noexcept {
            _canceled = true;
        }

        /**
         * @brief 清除取消标记.
         */
        constexpr void revive() noexcept {
            _canceled = false;
        }

        /**
         * @brief 判断动作是否已取消.
         *
         * @return true 动作已取消.
         * @return false 动作仍有效.
         */
        [[nodiscard]]
        constexpr bool canceled() const noexcept {
            return _canceled;
        }

    private:
        units::time _deadline{};
        bool _canceled = false;
    };

    /// pop_due 一次最多弹出的到期动作数（避免 ISR 内分配堆内存）
    constexpr size_t MAX_DUE_ACTIONS = 8;

    /**
     * @brief 一个到期动作队列条目.
     */
    struct ExpireActionEntry {
        units::time deadline{};
        util::owner<ExpireAction *> action = util::owner<ExpireAction *>(nullptr);
    };

    /**
     * @brief pop_due 的返回结果 —— 固定大小数组，不在 ISR 内分配堆内存.
     */
    struct PopDueResult {
        ExpireActionEntry entries[MAX_DUE_ACTIONS]{};
        size_t count = 0;
    };

    /**
     * @brief 保存到期动作的小根堆.
     */
    class ExpireActionQueue {
    public:
        /**
         * @brief 判断队列是否为空.
         *
         * @return true 队列为空.
         * @return false 队列非空.
         */
        [[nodiscard]]
        bool empty() const noexcept;

        /**
         * @brief 获取队列长度.
         *
         * @return size_t 当前队列中的动作数.
         */
        [[nodiscard]]
        size_t size() const noexcept;

        /**
         * @brief 插入一个到期动作.
         *
         * @param action 待插入动作所有权.
         * @return true 队列根节点发生变化.
         * @return false 队列根节点未变化.
         */
        [[nodiscard]]
        bool push(util::owner<ExpireAction *> action) noexcept;

        /**
         * @brief 查看当前最早到期动作.
         *
         * @return std::optional<ExpireActionEntry> 最早到期动作.
         */
        [[nodiscard]]
        std::optional<ExpireActionEntry> peek() const noexcept;

        /**
         * @brief 弹出所有 deadline 不晚于 now 的动作.
         *
         * @param now 当前时间.
         * @return std::vector<ExpireActionEntry> 已到期动作集合.
         */
        [[nodiscard]]
        PopDueResult pop_due(units::time now);

    private:
        static constexpr size_t MAX_HEAP_ACTIONS = 16;  // 调度 tick + sleep 定时器等

        /**
         * @brief 将指定节点向上调整以恢复小根堆性质.
         *
         * @param index 新插入节点下标.
         */
        void sift_up(size_t index) noexcept;

        /**
         * @brief 将指定节点向下调整以恢复小根堆性质.
         *
         * @param index 根节点下标.
         */
        void sift_down(size_t index) noexcept;

        /**
         * @brief 比较两个堆节点的到期时间.
         *
         * @param lhs 左节点.
         * @param rhs 右节点.
         * @return true 左节点应排在右节点之后.
         * @return false 左节点不晚于右节点.
         */
        [[nodiscard]]
        static bool later(const ExpireActionEntry &lhs,
                          const ExpireActionEntry &rhs) noexcept;

        ExpireActionEntry _heap[MAX_HEAP_ACTIONS]{};
        size_t _heap_size = 0;
    };

    /**
     * @brief 当前 hart 的到期事件管理器.
     */
    class TimeKeeper {
    public:
        /**
         * @brief 使用当前 hart 的时钟源与 Alarm 构造 TimeKeeper.
         *
         * @param source 当前 hart 的 ClockSource.
         * @param alarm 当前 hart 的 Alarm.
         */
        TimeKeeper(ClockSource *source, Alarm *alarm) noexcept;

        /**
         * @brief 新增一个到期动作.
         *
         * @param action 待提交动作所有权.
         */
        void enqueue(util::owner<ExpireAction *> action) noexcept;

        /**
         * @brief 处理一次 Alarm 到期中断.
         *
         * @param event 本次中断事件信息.
         */
        void on_timer_irq(const ClockEvent &event) noexcept;

        /**
         * @brief 获取底层时钟源.
         *
         * @return ClockSource* 底层时钟源.
         */
        [[nodiscard]]
        constexpr ClockSource *source() const noexcept {
            return _source;
        }

        /**
         * @brief 获取底层 Alarm.
         *
         * @return Alarm* 当前绑定的 Alarm.
         */
        [[nodiscard]]
        constexpr Alarm *alarm() const noexcept {
            return _alarm;
        }

    private:
        /**
         * @brief 根据当前队列根重新编程下一次 timer.
         */
        void rearm_timer() noexcept;

        /**
         * @brief 执行一个到期动作.
         *
         * @param action 待执行动作.
         * @param event 本次时钟事件信息.
         */
        static void run_action(ExpireAction &action,
                               const ClockEvent &event) noexcept;

        ClockSource *_source = nullptr;
        Alarm *_alarm = nullptr;
        ExpireActionQueue _queue{};
    };

}  // namespace device
