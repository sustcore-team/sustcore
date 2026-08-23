/**
 * @file ipi.cpp
 * @brief Runtime IPI 的无锁 mailbox 协议。
 */

#include <arch/interrupt.h>
#include <arch/smp.h>
#include <cpu/topology.h>
#include <scheduler/scheduler.h>
#include <smp/ipi.h>
#include <timer/hrtimer.h>

#include <atomic>

namespace smp {
    namespace detail::ipi {
        constexpr u32_t REASON_MASK = static_cast<u32_t>(IpiReason::RESCHEDULE) |
                                      static_cast<u32_t>(IpiReason::TLB_SHOOTDOWN) |
                                      static_cast<u32_t>(IpiReason::STOP) |
                                      static_cast<u32_t>(IpiReason::TIMER_DEADLINE);

        constinit std::atomic<TlbShootdownHandler> tlb_shootdown_handler{nullptr};

        [[nodiscard]] constexpr u32_t reason_bits(IpiReason reason) noexcept {
            return static_cast<u32_t>(reason);
        }

        [[nodiscard]] IpiMailbox *mailbox(cpu::CpuId target) noexcept {
            auto *storage = cpu::try_slot(target);
            return storage == nullptr ? nullptr : &storage->ipi;
        }

        [[nodiscard]] bool valid_reason(IpiReason reason) noexcept {
            const u32_t bits = reason_bits(reason);
            return bits != 0 && (bits & ~REASON_MASK) == 0;
        }

        [[nodiscard]] bool target_available(cpu::CpuId target, IpiReason reason) noexcept {
            const auto snapshot = cpu::topology().snapshot();
            return reason == IpiReason::STOP ? snapshot.started.test(target)
                                             : snapshot.online.test(target);
        }

        [[noreturn]] void stop_current_cpu() noexcept {
            hal::cli();
            while (true) hal::wfi();
        }
    }  // namespace detail::ipi

    bool post(cpu::CpuId target, IpiReason reason) noexcept {
        if (!detail::ipi::valid_reason(reason) || !detail::ipi::target_available(target, reason))
            return false;

        auto *target_mailbox = detail::ipi::mailbox(target);
        if (target_mailbox == nullptr)
            return false;

        const u32_t bits = detail::ipi::reason_bits(reason);
        const u32_t old  = target_mailbox->pending.fetch_or(bits, std::memory_order_release);
        target_mailbox->posted.fetch_add(1, std::memory_order_relaxed);
        if ((old & bits) == bits) {
            target_mailbox->coalesced.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        target_mailbox->notifications_needed.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    tay::expected<void, tay::error_code> request(cpu::CpuId target, IpiReason reason) noexcept {
        if (!detail::ipi::valid_reason(reason) || !detail::ipi::target_available(target, reason))
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        if (!post(target, reason))
            return {};
        return hal::send_ipi(cpu::topology().hw_id(target));
    }

    u32_t pending_reasons(cpu::CpuId target) noexcept {
        auto *target_mailbox = detail::ipi::mailbox(target);
        return target_mailbox == nullptr ? 0
                                         : target_mailbox->pending.load(std::memory_order_acquire);
    }

    IpiStats stats(cpu::CpuId target) noexcept {
        auto *target_mailbox = detail::ipi::mailbox(target);
        if (target_mailbox == nullptr)
            return {};
        return IpiStats{
            .posted    = target_mailbox->posted.load(std::memory_order_relaxed),
            .coalesced = target_mailbox->coalesced.load(std::memory_order_relaxed),
            .notifications_needed =
                target_mailbox->notifications_needed.load(std::memory_order_relaxed),
            .dispatches      = target_mailbox->dispatches.load(std::memory_order_relaxed),
            .reschedules     = target_mailbox->reschedules.load(std::memory_order_relaxed),
            .tlb_shootdowns  = target_mailbox->tlb_shootdowns.load(std::memory_order_relaxed),
            .timer_deadlines = target_mailbox->timer_deadlines.load(std::memory_order_relaxed),
            .stops           = target_mailbox->stops.load(std::memory_order_relaxed),
        };
    }

    void set_tlb_handler(TlbShootdownHandler handler) noexcept {
        if (handler == nullptr)
            __builtin_trap();
        TlbShootdownHandler expected = nullptr;
        if (!detail::ipi::tlb_shootdown_handler.compare_exchange_strong(
                expected, handler, std::memory_order_release, std::memory_order_relaxed))
            __builtin_trap();
    }

    void dispatch_ipi() noexcept {
        const cpu::CpuId current = cpu::current_id();
        auto *current_mailbox    = detail::ipi::mailbox(current);
        if (current_mailbox == nullptr)
            __builtin_trap();

        while (true) {
            const u32_t reasons = current_mailbox->pending.exchange(0, std::memory_order_acquire);
            if (reasons == 0)
                return;
            if ((reasons & ~detail::ipi::REASON_MASK) != 0)
                __builtin_trap();

            current_mailbox->dispatches.fetch_add(1, std::memory_order_relaxed);
            if ((reasons & detail::ipi::reason_bits(IpiReason::STOP)) != 0) {
                current_mailbox->stops.fetch_add(1, std::memory_order_relaxed);
                detail::ipi::stop_current_cpu();
            }
            if ((reasons & detail::ipi::reason_bits(IpiReason::TLB_SHOOTDOWN)) != 0) {
                const auto handler =
                    detail::ipi::tlb_shootdown_handler.load(std::memory_order_acquire);
                if (handler == nullptr)
                    __builtin_trap();
                current_mailbox->tlb_shootdowns.fetch_add(1, std::memory_order_relaxed);
                handler();
            }
            if ((reasons & detail::ipi::reason_bits(IpiReason::TIMER_DEADLINE)) != 0) {
                current_mailbox->timer_deadlines.fetch_add(1, std::memory_order_relaxed);
                kernel::timer::bsp_hrtimers().refresh_from_ipi();
            }
            if ((reasons & detail::ipi::reason_bits(IpiReason::RESCHEDULE)) != 0) {
                current_mailbox->reschedules.fetch_add(1, std::memory_order_relaxed);
                scheduler::local().request_reschedule(current);
            }
        }
    }
}  // namespace smp
