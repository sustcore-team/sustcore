/**
 * @file framework.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核分阶段 selftest 注册表与执行器。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <log.h>
#include <test/cases.h>
#include <test/framework.h>

#include <cstddef>

namespace kernel::test {
    namespace {
        using Body = void (*)(Context &) noexcept;

        struct TestCase final {
            const char *name = nullptr;
            Phase phase{};
            Body body = nullptr;
        };

        constexpr TestCase TEST_CASES[] = {
            {.name  = "error.model",
             .phase = Phase::POST_TIMER_INIT,
             .body  = cases::run_error_model},
            {.name  = "interrupt.registry",
             .phase = Phase::POST_TIMER_INIT,
             .body  = cases::run_irq_registry},
            {.name  = "interrupt.preempt_guard",
             .phase = Phase::POST_TIMER_INIT,
             .body  = cases::run_preempt_guard},
            {.name  = "cpu.topology",
             .phase = Phase::POST_TIMER_INIT,
             .body  = cases::run_cpu_topology},
            {.name  = "smp.ipi_mailbox",
             .phase = Phase::POST_SCHED_INIT,
             .body  = cases::run_ipi_mailbox},
            {.name  = "smp.tlb_shootdown",
             .phase = Phase::POST_SCHED_INIT,
             .body  = cases::run_tlb_shootdown},
            {.name  = "capability.cspace",
             .phase = Phase::POST_TIMER_INIT,
             .body  = cases::run_capability},
            {.name  = "scheduler.park",
             .phase = Phase::POST_SCHED_INIT,
             .body  = cases::run_scheduler_park},
            {.name  = "scheduler.rr_preemption",
             .phase = Phase::POST_SCHED_INIT,
             .body  = cases::run_rr_preemption},
            {.name  = "async.work_queue",
             .phase = Phase::POST_SCHED_INIT,
             .body  = cases::run_work_queue},
            {.name  = "timer.precision",
             .phase = Phase::POST_WORK_QUEUE_INITIALIZATION,
             .body  = cases::run_precise_timer},
            {.name  = "timer.periodic_probe",
             .phase = Phase::POST_WORK_QUEUE_INITIALIZATION,
             .body  = cases::run_timer_probe},
            {.name  = "smp.remote_scheduler_wake",
             .phase = Phase::POST_SMP_INIT,
             .body  = cases::run_remote_wake},
            {.name  = "smp.work_threads",
             .phase = Phase::POST_SMP_INIT,
             .body  = cases::run_smp_threads},
            {.name  = "smp.irq_synchronized_work_queue",
             .phase = Phase::POST_SMP_INIT,
             .body  = cases::run_smp_irq_queue},
            {.name  = "smp.allocator_and_object_stress",
             .phase = Phase::POST_SMP_INIT,
             .body  = cases::run_alloc_stress},
            {.name  = "scheduler.fifo_handoff",
             .phase = Phase::PRE_IDLE,
             .body  = cases::run_fifo_handoff},
        };

        constinit const char *current_case_name = nullptr;

        [[nodiscard]] constexpr const char *phase_name(Phase phase) noexcept {
            switch (phase) {
                case Phase::POST_TIMER_INIT:                return "post-timer-initialization";
                case Phase::POST_SCHED_INIT:                return "post-scheduler-initialization";
                case Phase::POST_WORK_QUEUE_INITIALIZATION: return "post-work-queue-initialization";
                case Phase::POST_SMP_INIT:                  return "post-smp-initialization";
                case Phase::PRE_IDLE:                       return "pre-idle";
            }
            return "unknown";
        }
    }  // namespace

    [[noreturn]] void fail(const char *message) noexcept {
        kernel::log::panic("Kernel selftest '{}' 失败: {}",
                           current_case_name == nullptr ? "<unregistered>" : current_case_name,
                           message == nullptr ? "未提供失败原因" : message);
    }

    void require(bool condition, const char *message) noexcept {
        if (!condition)
            fail(message);
    }

    void run_phase(Phase phase, Context ctx) noexcept {
        size_t executed = 0;
        kernel::log::info("[KTEST PHASE] {}", phase_name(phase));
        for (const auto &test_case : TEST_CASES) {
            if (test_case.phase != phase)
                continue;
            if (test_case.name == nullptr || test_case.body == nullptr)
                kernel::log::panic("Kernel selftest 注册表包含无效用例");

            current_case_name = test_case.name;
            kernel::log::info("[KTEST RUN ] {}", test_case.name);
            test_case.body(ctx);
            kernel::log::info("[KTEST PASS] {}", test_case.name);
            current_case_name = nullptr;
            ++executed;
        }
        kernel::log::info("[KTEST DONE] phase={}, passed={}", phase_name(phase), executed);
    }
}  // namespace kernel::test
