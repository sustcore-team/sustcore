/**
 * @file ipi.cpp
 * @brief Runtime IPI mailbox 的 BSP selftest。
 */

#include <arch/interrupt.h>
#include <cpu/local.h>
#include <scheduler/scheduler.h>
#include <smp/ipi.h>
#include <test/cases.h>

namespace kernel::test::cases {
    void run_ipi_mailbox(Context &) noexcept {
        const auto current = cpu::current_id();
        const auto before  = smp::stats(current);

        const bool interrupts_were_enabled = hal::irq_enabled();
        hal::cli();
        auto first = smp::request(current, smp::IpiReason::RESCHEDULE);
        require(first.has_value(), "first self IPI request must reach the architecture backend");
        auto duplicate = smp::request(current, smp::IpiReason::RESCHEDULE);
        require(duplicate.has_value(), "duplicate self IPI request must remain a valid merge");
        require(smp::pending_reasons(current) == static_cast<u32_t>(smp::IpiReason::RESCHEDULE),
                "self IPI mailbox must retain the reschedule reason");

        hal::sti();
        while (smp::stats(current).reschedules == before.reschedules) hal::wfi();
        if (!interrupts_were_enabled)
            hal::cli();

        const auto after = smp::stats(current);
        require(smp::pending_reasons(current) == 0,
                "IPI dispatch must consume all pending mailbox reasons");
        require(after.posted == before.posted + 2, "IPI statistics must count every mailbox post");
        require(after.coalesced == before.coalesced + 1,
                "IPI statistics must count merged duplicate posts");
        require(after.notifications_needed == before.notifications_needed + 1,
                "IPI statistics must distinguish a new pending reason from a merge");
        require(after.dispatches == before.dispatches + 1,
                "IPI mailbox must exchange the merged reasons in one dispatch pass");
        require(after.reschedules == before.reschedules + 1,
                "IPI dispatch must handle the reschedule reason exactly once");
        require(!scheduler::local().debug_state().need_reschedule,
                "IPI trap-return must complete the requested scheduler checkpoint");
    }
}  // namespace kernel::test::cases
