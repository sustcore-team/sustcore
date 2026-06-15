/**
 * @file clock.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 时钟
 * @version alpha-1.0.0
 * @date 2026-05-27
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/description.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/csr.h>
#endif
#include <driver/clock.h>
#include <driver/int/base.h>
#include <logger.h>

namespace driver {
    namespace {
        /**
         * @brief 复制一个到期动作条目但不转移所有权.
         *
         * @param entry 源条目.
         * @return ExpireActionEntry 仅用于只读查看的临时条目.
         */
        [[nodiscard]]
        ExpireActionEntry make_view_entry(
            const ExpireActionEntry &entry) noexcept {
            return ExpireActionEntry{
                .deadline = entry.deadline,
                .action   = util::owner<ExpireAction *>(entry.action.get()),
            };
        }
    }  // namespace

    /**
     * @brief 读取当前 CSR `time` 计数值.
     */
    units::tick CSRTimeClockSource::now() const noexcept {
        return csr_get_time();
    }

    /**
     * @brief 返回 CSR `time` 时钟源频率.
     */
    units::frequency CSRTimeClockSource::frequency() const noexcept {
        return _freq;
    }

    bool ExpireActionQueue::empty() const noexcept {
        InterruptGuard guard;
        guard.enter();
        return _heap.empty();
    }

    size_t ExpireActionQueue::size() const noexcept {
        InterruptGuard guard;
        guard.enter();
        return _heap.size();
    }

    bool ExpireActionQueue::push(util::owner<ExpireAction *> action) noexcept {
        if (action == nullptr) {
            loggers::DEVICE::ERROR("ExpireActionQueue::push 收到空动作");
            return false;
        }

        InterruptGuard guard;
        guard.enter();
        units::time old_root =
            _heap.empty() ? units::time{} : _heap.front().deadline;
        _heap.push_back(ExpireActionEntry{
            .deadline = action->deadline(),
            .action   = std::move(action),
        });
        sift_up(_heap.size() - 1);
        if (_heap.empty()) {
            return false;
        }
        return old_root.to_nanoseconds() == 0 ||
               _heap.front().deadline.to_nanoseconds() !=
                   old_root.to_nanoseconds();
    }

    std::optional<ExpireActionEntry> ExpireActionQueue::peek() const noexcept {
        InterruptGuard guard;
        guard.enter();
        if (_heap.empty()) {
            return std::nullopt;
        }
        return make_view_entry(_heap.front());
    }

    std::vector<ExpireActionEntry> ExpireActionQueue::pop_due(units::time now) {
        InterruptGuard guard;
        guard.enter();

        std::vector<ExpireActionEntry> due{};
        while (!_heap.empty() &&
               _heap.front().deadline.to_nanoseconds() <= now.to_nanoseconds())
        {
            due.push_back(std::move(_heap.front()));
            _heap.front() = std::move(_heap.back());
            _heap.pop_back();
            if (!_heap.empty()) {
                sift_down(0);
            }
        }
        return due;
    }

    void ExpireActionQueue::sift_up(size_t index) noexcept {
        while (index > 0) {
            size_t parent = (index - 1) / 2;
            if (!later(_heap[parent], _heap[index])) {
                break;
            }
            auto moved    = std::move(_heap[index]);
            _heap[index]  = std::move(_heap[parent]);
            _heap[parent] = std::move(moved);
            index         = parent;
        }
    }

    void ExpireActionQueue::sift_down(size_t index) noexcept {
        size_t heap_size = _heap.size();
        while (true) {
            size_t left     = index * 2 + 1;
            size_t right    = left + 1;
            size_t smallest = index;

            if (left < heap_size && later(_heap[smallest], _heap[left])) {
                smallest = left;
            }
            if (right < heap_size && later(_heap[smallest], _heap[right])) {
                smallest = right;
            }
            if (smallest == index) {
                break;
            }

            auto moved      = std::move(_heap[index]);
            _heap[index]    = std::move(_heap[smallest]);
            _heap[smallest] = std::move(moved);
            index           = smallest;
        }
    }

    bool ExpireActionQueue::later(const ExpireActionEntry &lhs,
                                  const ExpireActionEntry &rhs) noexcept {
        return lhs.deadline.to_nanoseconds() > rhs.deadline.to_nanoseconds();
    }

    TimeKeeper::TimeKeeper(ClockSource *source, Alarm *alarm) noexcept
        : _source(source), _alarm(alarm), _queue() {
        alarm->set_handler(this_call(this, &TimeKeeper::on_timer_irq));
    }

    void TimeKeeper::enqueue(util::owner<ExpireAction *> action) noexcept {
        if (action == nullptr) {
            loggers::TIMER::ERROR("TimeKeeper::enqueue 收到空动作");
            return;
        }
        if (_source == nullptr || _alarm == nullptr) {
            loggers::TIMER::ERROR("TimeKeeper 未绑定完整时钟设备");
            return;
        }

        units::time now = _source->to_ns(_source->now());
        if (action->deadline().to_nanoseconds() <= now.to_nanoseconds()) {
            loggers::TIMER::DEBUG(
                "TimeKeeper 直接执行已到期动作: deadline=%llu now=%llu",
                static_cast<unsigned long long>(
                    action->deadline().to_nanoseconds()),
                static_cast<unsigned long long>(now.to_nanoseconds()));
            ClockEvent event{.last = now, .now = now};
            run_action(*action, event);
            return;
        }

        bool root_changed = _queue.push(std::move(action));
        loggers::TIMER::DEBUG("TimeKeeper 入队动作: root_changed=%d size=%llu",
                               root_changed,
                               static_cast<unsigned long long>(_queue.size()));
        if (root_changed) {
            rearm_timer();
        }
    }

    void TimeKeeper::on_timer_irq(const ClockEvent &event) noexcept {
        if (_source == nullptr || _alarm == nullptr) {
            loggers::TIMER::ERROR("TimeKeeper 收到 timer IRQ 时未初始化");
            return;
        }

        units::time now = _source->to_ns(_source->now());
        auto due        = _queue.pop_due(now);
        for (auto &entry : due) {
            if (entry.action == nullptr) {
                continue;
            }
            run_action(*entry.action, event);
        }
        rearm_timer();
    }

    void TimeKeeper::rearm_timer() noexcept {
        if (_source == nullptr || _alarm == nullptr) {
            return;
        }

        auto next_opt = _queue.peek();
        if (!next_opt.has_value() || next_opt->action == nullptr) {
            loggers::TIMER::DEBUG("TimeKeeper 队列为空, 不重编程 timer");
            return;
        }

        units::time now      = _source->to_ns(_source->now());
        units::time deadline = next_opt->deadline;
        units::time delta    = deadline.to_nanoseconds() <= now.to_nanoseconds()
                                   ? units::time::from_nanoseconds(1)
                                   : deadline - now;
        loggers::TIMER::DEBUG(
            "TimeKeeper 重编程 timer: now=%llu deadline=%llu delta=%llu",
            static_cast<unsigned long long>(now.to_nanoseconds()),
            static_cast<unsigned long long>(deadline.to_nanoseconds()),
            static_cast<unsigned long long>(delta.to_nanoseconds()));
        _alarm->set_next_event(delta);
    }

    void TimeKeeper::run_action(ExpireAction &action,
                                const ClockEvent &event) noexcept {
        if (action.canceled()) {
            loggers::DEVICE::DEBUG("TimeKeeper 跳过已取消动作: deadline=%llu",
                                   static_cast<unsigned long long>(
                                       action.deadline().to_nanoseconds()));
            return;
        }
        action.expire(event);
    }
}  // namespace device
