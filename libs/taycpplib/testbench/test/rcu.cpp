/**
 * @file rcu.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay RCU 读侧、同步回收和并发 writer 语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/rcu.h>

#include <array>
#include <atomic>
#include <cassert>
#include <thread>
#include <type_traits>
#include <utility>

namespace {
    struct host_policy {
        void enter_read() noexcept {
            readers.fetch_add(1, std::memory_order_acq_rel);
        }

        void exit_read() noexcept {
            readers.fetch_sub(1, std::memory_order_acq_rel);
        }

        void synchronize() noexcept {
            synchronizations.fetch_add(1, std::memory_order_relaxed);
            while (readers.load(std::memory_order_acquire) != 0) {
                std::this_thread::yield();
            }
        }

        static void reset() noexcept {
            readers.store(0, std::memory_order_relaxed);
            synchronizations.store(0, std::memory_order_relaxed);
        }

        static inline std::atomic<size_t> readers{0};
        static inline std::atomic<size_t> synchronizations{0};
    };

    struct retired_value : tay::rcu_retired_node {
        std::atomic<size_t> *reclaimed = nullptr;
    };

    void reclaim_value(tay::rcu_retired_node &node) noexcept {
        auto &value = static_cast<retired_value &>(node);
        value.reclaimed->fetch_add(1, std::memory_order_relaxed);
    }

    using domain_type = tay::rcu_domain<host_policy, 32>;
    using guard_type  = decltype(std::declval<domain_type &>().read_lock());

    static_assert(tay::RcuPolicy<host_policy>);
    static_assert(!std::is_copy_constructible_v<guard_type>);
    static_assert(!std::is_move_constructible_v<guard_type>);
    static_assert(!std::is_copy_constructible_v<domain_type>);
    static_assert(!std::is_move_constructible_v<domain_type>);
    static_assert(noexcept(std::declval<domain_type &>().read_lock()));

    void test_nested_readers() {
        host_policy::reset();
        domain_type domain;
        assert(host_policy::readers.load(std::memory_order_relaxed) == 0);
        {
            auto outer = domain.read_lock();
            assert(host_policy::readers.load(std::memory_order_relaxed) == 1);
            {
                auto inner = domain.read_lock();
                assert(host_policy::readers.load(std::memory_order_relaxed) == 2);
                static_cast<void>(inner);
            }
            assert(host_policy::readers.load(std::memory_order_relaxed) == 1);
            static_cast<void>(outer);
        }
        assert(host_policy::readers.load(std::memory_order_relaxed) == 0);
    }

    void test_synchronous_reclamation() {
        host_policy::reset();
        domain_type domain;
        std::atomic<size_t> reclaimed{0};
        std::array<retired_value, 3> values{};
        for (auto &value : values) {
            value.reclaimed = &reclaimed;
            assert(domain.retire(value, reclaim_value));
        }
        assert(reclaimed.load(std::memory_order_relaxed) == 0);

        domain.synchronize();
        assert(reclaimed.load(std::memory_order_relaxed) == values.size());
        assert(host_policy::synchronizations.load(std::memory_order_relaxed) == 1);
    }

    void test_capacity_and_duplicate_retire() {
        host_policy::reset();
        tay::rcu_domain<host_policy, 2> domain;
        std::atomic<size_t> reclaimed{0};
        std::array<retired_value, 3> values{};
        for (auto &value : values) {
            value.reclaimed = &reclaimed;
        }

        auto null_callback = domain.retire(values[0], nullptr);
        assert(!null_callback && null_callback.error() == tay::error_code::NULLPTR);
        assert(domain.retire(values[0], reclaim_value));

        auto duplicate = domain.retire(values[0], reclaim_value);
        assert(!duplicate && duplicate.error() == tay::error_code::INVALID_ARGUMENT);
        assert(domain.retire(values[1], reclaim_value));

        auto overflow = domain.retire(values[2], reclaim_value);
        assert(!overflow && overflow.error() == tay::error_code::OVERFLOW_ERROR);

        domain.synchronize();
        assert(reclaimed.load(std::memory_order_relaxed) == 2);
    }

    void test_node_reuse_after_callback() {
        host_policy::reset();
        domain_type domain;
        std::atomic<size_t> reclaimed{0};
        retired_value value;
        value.reclaimed = &reclaimed;

        assert(domain.retire(value, reclaim_value));
        domain.synchronize();
        assert(reclaimed.load(std::memory_order_relaxed) == 1);

        assert(domain.retire(value, reclaim_value));
        domain.synchronize();
        assert(reclaimed.load(std::memory_order_relaxed) == 2);
    }

    void test_reader_delays_reclamation() {
        host_policy::reset();
        domain_type domain;
        std::atomic<size_t> reclaimed{0};
        std::atomic<bool> reader_ready{false};
        std::atomic<bool> release_reader{false};
        std::atomic<bool> synchronize_started{false};
        retired_value value;
        value.reclaimed = &reclaimed;

        std::thread reader([&] {
            auto held = domain.read_lock();
            reader_ready.store(true, std::memory_order_release);
            while (!release_reader.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            static_cast<void>(held);
        });
        while (!reader_ready.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        assert(domain.retire(value, reclaim_value));
        std::thread writer([&] {
            synchronize_started.store(true, std::memory_order_release);
            domain.synchronize();
        });
        while (!synchronize_started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        while (host_policy::synchronizations.load(std::memory_order_acquire) == 0) {
            std::this_thread::yield();
        }
        assert(reclaimed.load(std::memory_order_relaxed) == 0);

        release_reader.store(true, std::memory_order_release);
        reader.join();
        writer.join();
        assert(reclaimed.load(std::memory_order_relaxed) == 1);
    }

    void test_concurrent_writers() {
        host_policy::reset();
        domain_type domain;
        std::atomic<size_t> reclaimed{0};
        constexpr size_t writer_count     = 4;
        constexpr size_t nodes_per_writer = 8;
        std::array<retired_value, writer_count * nodes_per_writer> values{};
        for (auto &value : values) {
            value.reclaimed = &reclaimed;
        }

        std::array<std::thread, writer_count> writers;
        for (size_t writer = 0; writer < writer_count; ++writer) {
            writers[writer] = std::thread([&, writer] {
                const auto begin = writer * nodes_per_writer;
                const auto end   = begin + nodes_per_writer;
                for (size_t index = begin; index < end; ++index) {
                    assert(domain.retire(values[index], reclaim_value));
                    if ((index - begin) % 3 == 2) {
                        domain.synchronize();
                    }
                }
            });
        }
        for (auto &writer : writers) {
            writer.join();
        }

        domain.synchronize();
        assert(reclaimed.load(std::memory_order_relaxed) == values.size());
    }
}  // namespace

int main() {
    test_nested_readers();
    test_synchronous_reclamation();
    test_capacity_and_duplicate_retire();
    test_node_reuse_after_callback();
    test_reader_delays_reclamation();
    test_concurrent_writers();
    return 0;
}
