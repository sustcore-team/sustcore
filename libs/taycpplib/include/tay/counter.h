/**
 * @file counter.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供可常量初始化的通用原子计数器。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <atomic>
#include <limits>
#include <type_traits>

namespace tay {
    /**
     * @brief 对整数值执行原子计数和单调序号分配。
     *
     * `counter` 不规定计数值的业务含义，也不自动处理回绕。调用方可以使用 `next()`
     * 获取无界序号，或使用 `try_next()` 在可表达的业务上限内分配序号。对象可通过
     * `constinit` 建立，适合内核启动阶段的全局 ID allocator。
     *
     * @tparam T 非 bool 的整数计数类型。
     * @note 单次操作是原子的，但是否 lock-free 取决于目标平台的 `std::atomic<T>`；多个
     * 计数器之间不提供事务语义。
     */
    template <typename T>
        requires(std::is_integral_v<T> && !std::is_same_v<std::remove_cv_t<T>, bool>)
    class counter final {
    public:
        using value_type = T;

        /** @brief 构造从零开始的计数器。 */
        constexpr counter() noexcept = default;

        /**
         * @brief 以指定的下一可用值构造计数器。
         * @param initial 首次 `next()` 返回的值。
         */
        constexpr explicit counter(value_type initial) noexcept : value_(initial) {}

        counter(const counter &)            = delete;
        counter &operator=(const counter &) = delete;
        counter(counter &&)                 = delete;
        counter &operator=(counter &&)      = delete;

        /**
         * @brief 读取当前计数值。
         * @param order 原子 load 使用的内存序。
         * @return 下一次 `next()` 将返回的值。
         */
        [[nodiscard]] value_type value(
            std::memory_order order = std::memory_order_relaxed) const noexcept {
            return value_.load(order);
        }

        /**
         * @brief 覆盖计数值。
         * @param value 后续操作使用的新值。
         * @param order 原子 store 使用的内存序。
         * @warning 调用方必须排除与并发序号分配之间的竞态。
         */
        void reset(value_type value        = value_type{},
                   std::memory_order order = std::memory_order_relaxed) noexcept {
            value_.store(value, order);
        }

        /**
         * @brief 返回当前值并将计数器递增一。
         * @param order 原子 read-modify-write 使用的内存序。
         * @return 本次分配到的计数值。
         * @pre 当前值不能是 `value_type` 的最大值。
         */
        [[nodiscard]] value_type next(
            std::memory_order order = std::memory_order_relaxed) noexcept {
            return value_.fetch_add(value_type{1}, order);
        }

        /**
         * @brief 在计数值不超过业务上限时分配下一个值。
         * @param maximum 允许返回的最大值，包含该端点。
         * @param output 成功时接收分配到的值；失败时保持不变。
         * @param order 成功 CAS 使用的内存序。
         * @return 成功分配返回 true；计数器已经越过上限时返回 false。
         * @pre `maximum` 必须小于 `value_type` 的最大值，以便保存耗尽状态。
         */
        [[nodiscard]] bool try_next(value_type maximum, value_type &output,
                                    std::memory_order order = std::memory_order_relaxed) noexcept {
            if (maximum == std::numeric_limits<value_type>::max())
                return false;

            auto current = value_.load(std::memory_order_relaxed);
            while (current <= maximum) {
                if (value_.compare_exchange_weak(current, static_cast<value_type>(current + 1),
                                                 order, std::memory_order_relaxed))
                {
                    output = current;
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 将计数器递增一并返回递增后的值。
         * @param order 原子 read-modify-write 使用的内存序。
         * @return 递增后的计数值。
         */
        [[nodiscard]] value_type increment(
            std::memory_order order = std::memory_order_relaxed) noexcept {
            return static_cast<value_type>(value_.fetch_add(value_type{1}, order) + 1);
        }

        /**
         * @brief 将计数器递减一并返回递减后的值。
         * @param order 原子 read-modify-write 使用的内存序。
         * @return 递减后的计数值。
         * @pre 当前值必须大于零。
         */
        [[nodiscard]] value_type decrement(
            std::memory_order order = std::memory_order_relaxed) noexcept {
            return static_cast<value_type>(value_.fetch_sub(value_type{1}, order) - 1);
        }

    private:
        std::atomic<value_type> value_{};
    };
}  // namespace tay
