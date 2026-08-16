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
    void run_error_model(Context &context) noexcept;
    void run_interrupt_registry(Context &context) noexcept;
    void run_capability(Context &context) noexcept;
    void run_scheduler_park(Context &context) noexcept;
    void run_rr_preemption(Context &context) noexcept;
    void run_work_queue(Context &context) noexcept;
    void run_precision_timer(Context &context) noexcept;
    void run_periodic_timer_probe(Context &context) noexcept;
    void run_fifo_handoff(Context &context) noexcept;
}  // namespace kernel::test::cases
