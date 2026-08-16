/**
 * @file rcu.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供由上层策略定义 grace period 的通用 RCU 基础设施。
 * @version 0.1.0-dev.1
 * @date 2026-08-15
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/err.h>
#include <tay/expected.h>
#include <tay/list.h>
#include <tay/lock.h>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace tay {
    /**
     * @brief 约束 RCU domain 使用的 reader 跟踪和 grace period 策略。
     *
     * Tay 不假设 TLS、CPU ID、调度器或 futex。具体策略必须负责 reader
     * 嵌套语义，并保证 synchronize() 等待调用前已存在的 reader 离开。
     */
    template <class Policy>
    concept RcuPolicy = requires(Policy &policy) {
        {
            policy.enter_read()
        } noexcept -> std::same_as<void>;
        {
            policy.exit_read()
        } noexcept -> std::same_as<void>;
        {
            policy.synchronize()
        } noexcept -> std::same_as<void>;
    };

    struct rcu_retired_node {
        using hook_type  = intrusive_list_hook<rcu_retired_node *, rcu_retired_node *>;
        using reclaim_fn = void (*)(rcu_retired_node &) noexcept;

        hook_type hook{};
        reclaim_fn reclaim = nullptr;
    };

    template <class Domain>
    class rcu_read_guard {
        friend Domain;

    public:
        rcu_read_guard(const rcu_read_guard &)            = delete;
        rcu_read_guard &operator=(const rcu_read_guard &) = delete;
        rcu_read_guard(rcu_read_guard &&)                 = delete;
        rcu_read_guard &operator=(rcu_read_guard &&)      = delete;

        ~rcu_read_guard() noexcept {
            domain_->exit_read();
        }

    private:
        explicit rcu_read_guard(Domain &domain) noexcept : domain_(&domain) {
            domain_->enter_read();
        }

        Domain *domain_;
    };

    /**
     * @brief 固定待回收容量、同步执行回收的 policy-based RCU domain。
     *
     * 对象发布和读取仍由调用方通过原子指针完成。所有已 retire 的节点必须在
     * domain 销毁前完成 synchronize()，且节点从 retire 成功起至 reclaim callback
     * 开始执行前不得由调用方修改。
     */
    template <RcuPolicy Policy, size_t MaxRetired>
    class rcu_domain {
        static_assert(MaxRetired > 0, "rcu_domain requires a non-zero retired capacity");

        using retired_list = intrusive_list<
            rcu_retired_node,
            locate_member<rcu_retired_node, rcu_retired_node::hook_type, &rcu_retired_node::hook>>;

        friend class rcu_read_guard<rcu_domain>;

    public:
        using policy_type = Policy;
        using read_guard  = rcu_read_guard<rcu_domain>;

        constexpr rcu_domain() noexcept(std::is_nothrow_default_constructible_v<Policy>)
            requires std::default_initializable<Policy>
        = default;

        template <class... Args>
            requires std::constructible_from<Policy, Args...>
        explicit constexpr rcu_domain(in_place_t, Args &&...args) noexcept(
            std::is_nothrow_constructible_v<Policy, Args...>)
            : policy_(std::forward<Args>(args)...) {}

        rcu_domain(const rcu_domain &)            = delete;
        rcu_domain &operator=(const rcu_domain &) = delete;
        rcu_domain(rcu_domain &&)                 = delete;
        rcu_domain &operator=(rcu_domain &&)      = delete;

        /**
         * @brief 进入无分配、无阻塞的 RCU 读侧临界区。
         *
         * guard 可用于硬中断上下文，其生命周期必须覆盖对受保护对象的全部访问。
         */
        [[nodiscard]] read_guard read_lock() noexcept {
            return read_guard(*this);
        }

        /**
         * @brief 将节点加入下一次 grace period 后的同步回收批次。
         *
         * 不允许在硬中断上下文调用。reclaim callback 只会在 synchronize() 完成
         * grace period 后的普通写侧上下文执行，必须为 noexcept，且不得依赖硬中断
         * 上下文。成功后，调用方在 callback 开始执行前不得修改或销毁 node。
         */
        [[nodiscard]] expected<void, error_code> retire(
            rcu_retired_node &node, rcu_retired_node::reclaim_fn reclaim) noexcept {
            if (reclaim == nullptr) {
                return expected<void, error_code>(unexpect, error_code::NULLPTR);
            }

            lock_guard held{writer_lock_};
            if (node.hook.in_list) {
                return expected<void, error_code>(unexpect, error_code::INVALID_ARGUMENT);
            }
            if (retired_count_ == MaxRetired) {
                return expected<void, error_code>(unexpect, error_code::OVERFLOW_ERROR);
            }

            node.reclaim = reclaim;
            retired_.push_back(&node);
            ++retired_count_;
            return {};
        }

        /**
         * @brief 等待一个 grace period，并同步回收调用前已经 retire 的节点。
         *
         * 不允许在硬中断上下文调用。本函数可能等待既有 reader 离开，并在当前
         * 普通写侧上下文直接执行 reclaim callback。等待期间的新 retire 会进入下一批。
         */
        void synchronize() noexcept {
            lock_guard synchronizing{synchronize_lock_};
            retired_list pending;
            {
                lock_guard held{writer_lock_};
                pending.splice(pending.end(), retired_);
                retired_count_ = 0;
            }

            policy_.synchronize();
            while (!pending.empty()) {
                auto *node    = pending.front();
                auto reclaim  = node->reclaim;
                node->reclaim = nullptr;
                static_cast<void>(pending.pop_front());
                reclaim(*node);
            }
        }

    private:
        void enter_read() noexcept {
            policy_.enter_read();
        }

        void exit_read() noexcept {
            policy_.exit_read();
        }

        [[no_unique_address]] Policy policy_{};
        // synchronize 独立串行化，grace period 等待期间仍允许 retire 进入下一批。
        spinlock synchronize_lock_;
        spinlock writer_lock_;

        // TODO: 异步回收和多生产者 retire 开放后，应迁移为 MPSC retired queue。
        retired_list retired_;
        size_t retired_count_ = 0;
    };
}  // namespace tay
