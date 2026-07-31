/**
 * @file spinlock.h
 * @brief Freestanding spin locks.
 * @version 0.1.0-dev.1
 * @date 2026-08-01
 */

#pragma once

#include <atomic>
#include <cstdint>

namespace tay {
    namespace detail {
        inline void spin_wait_hint() noexcept {
#if defined(__i386__) || defined(__x86_64__)
            __builtin_ia32_pause();
#else
            __asm__ __volatile__("" ::: "memory");
#endif
        }
    }  // namespace detail

    class spinlock {
    public:
        constexpr spinlock() noexcept = default;

        spinlock(const spinlock&)            = delete;
        spinlock& operator=(const spinlock&) = delete;
        spinlock(spinlock&&)                 = delete;
        spinlock& operator=(spinlock&&)      = delete;

        void lock() noexcept {
            while (flag_.test_and_set(std::memory_order_acquire)) {
                while (flag_.test(std::memory_order_relaxed)) {
                    detail::spin_wait_hint();
                }
            }
        }

        [[nodiscard]] bool try_lock() noexcept {
            return !flag_.test_and_set(std::memory_order_acquire);
        }

        void unlock() noexcept {
            flag_.clear(std::memory_order_release);
        }

    private:
        std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
    };

    class ticket_spinlock {
    public:
        constexpr ticket_spinlock() noexcept = default;

        ticket_spinlock(const ticket_spinlock&)            = delete;
        ticket_spinlock& operator=(const ticket_spinlock&) = delete;
        ticket_spinlock(ticket_spinlock&&)                 = delete;
        ticket_spinlock& operator=(ticket_spinlock&&)      = delete;

        void lock() noexcept {
            const auto ticket =
                next_ticket_.fetch_add(1, std::memory_order_relaxed);
            while (serving_ticket_.load(std::memory_order_acquire) != ticket) {
                detail::spin_wait_hint();
            }
        }

        [[nodiscard]] bool try_lock() noexcept {
            const auto serving =
                serving_ticket_.load(std::memory_order_acquire);
            auto expected = serving;
            return next_ticket_.compare_exchange_strong(
                expected, serving + 1, std::memory_order_acquire,
                std::memory_order_relaxed);
        }

        void unlock() noexcept {
            const auto serving =
                serving_ticket_.load(std::memory_order_relaxed);
            serving_ticket_.store(serving + 1, std::memory_order_release);
        }

    private:
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                      "ticket_spinlock requires lock-free 32-bit atomics");

        std::atomic<std::uint32_t> next_ticket_{0};
        std::atomic<std::uint32_t> serving_ticket_{0};
    };
}  // namespace tay
