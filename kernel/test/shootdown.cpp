/**
 * @file shootdown.cpp
 * @brief BSP TLB shootdown generation 与本地 acknowledgement selftest。
 */

#include <arch/interrupt.h>
#include <arch/paging_traits.h>
#include <cpu/local.h>
#include <memory/virtual/kernel/vm.h>
#include <smp/ipi.h>
#include <smp/shootdown.h>
#include <test/cases.h>

namespace kernel::test::cases {
    void run_tlb_shootdown(Context &) noexcept {
        const auto current         = cpu::current_id();
        const auto before          = smp::shootdown_snapshot();
        const u64_t flushes_before = hal::PtOps::debug_flushes();

        smp::shootdown(memory::kernel_vm().binding(), 0, 0);

        const auto after = smp::shootdown_snapshot();
        require(after.generation == before.generation + 1,
                "TLB shootdown must publish a monotonic generation");
        require(after.targets.test(current),
                "TLB shootdown target snapshot must contain the initiating CPU");
        require(smp::acked_gen(current) >= after.generation,
                "initiating CPU must acknowledge its local TLB flush before returning");
        require(hal::PtOps::debug_flushes() == flushes_before + 1,
                "BSP-only TLB shootdown must execute exactly one local flush");

        const auto ipi_before              = smp::stats(current);
        const u64_t flushes_after_local    = hal::PtOps::debug_flushes();
        const bool interrupts_were_enabled = hal::irq_enabled();
        hal::cli();
        auto request = smp::request(current, smp::IpiReason::TLB_SHOOTDOWN);
        require(request.has_value(), "TLB self IPI request must reach the architecture backend");
        hal::sti();
        while (smp::stats(current).tlb_shootdowns == ipi_before.tlb_shootdowns) hal::wfi();
        if (!interrupts_were_enabled)
            hal::cli();

        const auto ipi_after = smp::stats(current);
        require(smp::pending_reasons(current) == 0,
                "TLB IPI dispatch must consume the mailbox reason");
        require(ipi_after.tlb_shootdowns == ipi_before.tlb_shootdowns + 1,
                "TLB IPI handler must execute exactly once");
        require(hal::PtOps::debug_flushes() == flushes_after_local + 1,
                "TLB IPI handler must execute one local flush");
        require(smp::acked_gen(current) >= after.generation,
                "TLB IPI handler must retain the published generation acknowledgement");
    }
}  // namespace kernel::test::cases
