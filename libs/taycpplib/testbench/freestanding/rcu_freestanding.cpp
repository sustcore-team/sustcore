/**
 * @file rcu_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 Tay RCU 基础设施可在 freestanding 环境中独立编译。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/rcu.h>

#include <type_traits>
#include <utility>

namespace {
    struct fake_policy {
        void enter_read() noexcept {
            ++readers;
        }

        void exit_read() noexcept {
            --readers;
        }

        void synchronize() noexcept {
            ++synchronizations;
        }

        size_t readers          = 0;
        size_t synchronizations = 0;
    };

    struct retired_value : tay::rcu_retired_node {
        bool reclaimed = false;
    };

    void reclaim(tay::rcu_retired_node &node) noexcept {
        static_cast<retired_value &>(node).reclaimed = true;
    }

    using domain_type = tay::rcu_domain<fake_policy, 4>;
    using guard_type  = decltype(std::declval<domain_type &>().read_lock());

    static_assert(tay::RcuPolicy<fake_policy>);
    static_assert(!std::is_copy_constructible_v<guard_type>);
    static_assert(!std::is_move_constructible_v<guard_type>);
    static_assert(!std::is_copy_constructible_v<domain_type>);
    static_assert(!std::is_move_constructible_v<domain_type>);
    static_assert(noexcept(std::declval<domain_type &>().read_lock()));
}  // namespace

void tay_rcu_freestanding_contract() {
    domain_type domain;
    retired_value value;

    {
        auto outer = domain.read_lock();
        auto inner = domain.read_lock();
        static_cast<void>(outer);
        static_cast<void>(inner);
    }

    auto retired = domain.retire(value, reclaim);
    if (retired) {
        domain.synchronize();
    }
}
