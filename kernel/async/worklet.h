/**
 * @file worklet.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 宿主嵌入、侵入式排队的延后工作项。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/bits.h>
#include <tay/list.h>

#include <atomic>
#include <type_traits>

namespace kernel::timer {
    class PrecisionTimerEngine;
}

namespace kernel::async {
    class WorkQueue;

    /**
     * @brief 由宿主嵌入并拥有、由 WorkQueue 临时借用的延后工作项。
     *
     * producer 在填充具体派生类字段后，通过 IDLE -> RESERVED/QUEUED 的 release
     * 转换发布它们。WorkQueue 在虚调用前完成摘链并转回 IDLE；run() 可以重新
     * post 自身，也可以间接销毁整个宿主。因此 run() 是 WorkQueue 对本对象的最后一次
     * 访问。第一次 reservation/post 会把 Worklet 永久绑定到该 WorkQueue；这保证
     * self-repost 仍由同一 worker 串行执行，避免首次 run() 返回前发生跨队列重入。
     *
     * RESERVED/QUEUED 期间不得修改派生类字段。进入 run() 前的 IDLE 只表示队列
     * 借用已结束，执行所有权此时线性移交给 run()；除当前 run() 本身外，其他
     * producer 必须等待 run() 返回或通过独立的宿主同步协议取得所有权，才能重新
     * 配置或 post 本对象。同一 Worklet 不得被多个 producer 并发配置或 post。
     */
    class Worklet {
    public:
        using queue_hook = tay::intrusive_list_hook<Worklet *, Worklet *>;
        queue_hook queue_hook_{};

        Worklet(const Worklet &)            = delete;
        Worklet &operator=(const Worklet &) = delete;
        Worklet(Worklet &&)                 = delete;
        Worklet &operator=(Worklet &&)      = delete;

        /**
         * @brief 查询 timer reservation 或 WorkQueue 是否仍借用本对象。
         *
         * 返回 false 不是跨 CPU 销毁许可；tail-dispatch 会在 run() 之前转回 IDLE。
         * 因此该查询也不能证明 run() 已经返回。宿主仍必须通过 pin、引用或
         * operation completion 协议保证生命期和配置字段的独占访问。
         */
        [[nodiscard]] bool pending() const noexcept;

    protected:
        constexpr Worklet() noexcept = default;

        /**
         * @brief WorkQueue 不通过基类指针删除 Worklet。
         *
         * protected 非虚析构允许派生类随宿主正常析构，并禁止 delete Worklet *。
         */
        ~Worklet() noexcept;

    private:
        enum class State : u8_t {
            IDLE,
            RESERVED,
            QUEUED,
        };

        static_assert(std::atomic<State>::is_always_lock_free,
                      "Worklet state must remain lock-free for IRQ producers");
        static_assert(std::atomic<WorkQueue *>::is_always_lock_free,
                      "Worklet queue affinity must remain lock-free for IRQ producers");

        /**
         * @brief 由具体 Worklet 实现 typed dispatch。
         * @warning 可能销毁宿主的 completion、unpin 或 coroutine resume 必须是最后一步。
         */
        virtual void run() noexcept = 0;

        [[nodiscard]] bool try_reserve_for_timer(WorkQueue &queue) noexcept;
        [[nodiscard]] bool try_claim_for_queue(WorkQueue &queue) noexcept;
        [[nodiscard]] bool try_claim_reserved_for_queue(WorkQueue &queue) noexcept;
        [[nodiscard]] bool try_claim(State expected, State desired, WorkQueue &queue) noexcept;

        /**
         * @brief 在 queue hook 已摘除后，将 QUEUED 线性移交给 run()。
         *
         * acquire 观察 producer 发布的派生类字段。转换完成后 WorkQueue 只能执行
         * run()，并且在虚调用返回后不得再访问 this。
         */
        void release_to_dispatch() noexcept;

        std::atomic<State> state_{State::IDLE};
        // 仅保存稳定的 queue 身份，不表示 Worklet 拥有或延长 WorkQueue 生命周期。
        std::atomic<WorkQueue *> queue_{nullptr};

        friend class WorkQueue;
        friend class kernel::timer::PrecisionTimerEngine;
        friend struct WorkletHookLocator;
    };

    static_assert(!std::is_copy_constructible_v<Worklet>);
    static_assert(!std::is_copy_assignable_v<Worklet>);
    static_assert(!std::is_move_constructible_v<Worklet>);
    static_assert(!std::is_move_assignable_v<Worklet>);

    using worklet_list = tay::intrusive_list<
        Worklet, tay::locate_member<Worklet, Worklet::queue_hook, &Worklet::queue_hook_>>;
}  // namespace kernel::async
