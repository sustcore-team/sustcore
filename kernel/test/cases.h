/**
 * @file cases.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核 selftest 用例入口声明。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <test/framework.h>

namespace kernel::test::cases {
    void run_error_model(Context &ctx) noexcept;
    void run_irq_registry(Context &ctx) noexcept;
    void run_preempt_guard(Context &ctx) noexcept;
    void run_cpu_topology(Context &ctx) noexcept;
    void run_ipi_mailbox(Context &ctx) noexcept;
    void run_tlb_shootdown(Context &ctx) noexcept;
    void run_capability(Context &ctx) noexcept;
    void run_scheduler_park(Context &ctx) noexcept;
    void run_rr_preemption(Context &ctx) noexcept;
    void run_work_queue(Context &ctx) noexcept;
    void run_precise_timer(Context &ctx) noexcept;
    void run_timer_probe(Context &ctx) noexcept;
    void run_remote_wake(Context &ctx) noexcept;
    void run_smp_threads(Context &ctx) noexcept;
    void run_smp_irq_queue(Context &ctx) noexcept;
    void run_alloc_stress(Context &ctx) noexcept;
    void run_fifo_handoff(Context &ctx) noexcept;
}  // namespace kernel::test::cases
