/**
 * @file scheduler_fifo.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kinit 与 worker Thread 的 FIFO handoff 启动 selftest。
 * @version 0.1.0-dev.1
 * @date 2026-08-17
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <log.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>
#include <test/cases.h>

#include <cstddef>
#include <utility>

namespace kernel::test::cases {
    namespace {
        constexpr size_t WORKER_COUNT      = 8;
        constexpr size_t PARTICIPANT_COUNT = WORKER_COUNT + 1;
        constexpr size_t ITERATION_COUNT   = 10;

        struct FifoState final {
            size_t expected_participant = 0;
            size_t completed[PARTICIPANT_COUNT]{};
        };

        struct WorkerArgument final {
            FifoState *state = nullptr;
            size_t participant;
            const char *name = nullptr;
        };

        void run_participant(FifoState &state, size_t participant, const char *name) noexcept {
            for (size_t iteration = 0; iteration < ITERATION_COUNT; ++iteration) {
                // 新 Thread 在 entry 前已可被 RR 抢占；turn predicate 将该合法到达抖动与
                // 显式 yield 的有序 handoff 分离，RR 抢占本身由独立 selftest 覆盖。
                while (state.expected_participant != participant) scheduler::yield();
                hal::interrupt_guard interrupt_guard;
                if (state.expected_participant != participant ||
                    state.completed[participant] != iteration)
                    kernel::test::fail("FIFO handoff 状态与当前参与者不一致");
                kernel::log::info("FIFO 调度测试: {} {}/{}", name, iteration + 1, ITERATION_COUNT);
                ++state.completed[participant];
                state.expected_participant = (participant + 1) % PARTICIPANT_COUNT;
                scheduler::yield();
            }
        }

        void worker_entry(void *opaque) noexcept {
            auto *argument = static_cast<WorkerArgument *>(opaque);
            if (argument == nullptr || argument->state == nullptr || argument->name == nullptr)
                kernel::test::fail("无效的 FIFO worker 参数");
            run_participant(*argument->state, argument->participant, argument->name);
        }
    }  // namespace

    void run_fifo_handoff(Context &context) noexcept {
        kernel::test::require(context.current_thread != nullptr,
                              "FIFO handoff 用例缺少 kinit Thread");

        FifoState state{};
        constexpr const char *WORKER_NAMES[WORKER_COUNT] = {
            "worker-1", "worker-2", "worker-3", "worker-4",
            "worker-5", "worker-6", "worker-7", "worker-8",
        };
        WorkerArgument worker_arguments[WORKER_COUNT]{};
        cap::ObjectRef<task::Thread> workers[WORKER_COUNT]{};

        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            worker_arguments[index] = WorkerArgument{
                .state = &state, .participant = index + 1, .name = WORKER_NAMES[index]};
            auto worker = task::Thread::create_kernel(task::kernel_process(), worker_entry,
                                                      &worker_arguments[index]);
            if (!worker)
                kernel::test::fail("无法创建 FIFO worker");
            workers[index] = std::move(*worker);
            if (auto resumed = scheduler::instance().attach(*workers[index]); !resumed)
                kernel::test::fail("无法发布 FIFO worker");
        }

        run_participant(state, 0, "kinit");

        for (;;) {
            bool all_exited = true;
            for (const auto &worker : workers) {
                if (!worker->exited()) {
                    all_exited = false;
                    break;
                }
            }
            if (all_exited)
                break;
            scheduler::yield();
        }

        for (size_t participant = 0; participant < PARTICIPANT_COUNT; ++participant) {
            if (state.completed[participant] != ITERATION_COUNT)
                kernel::test::fail("FIFO 调度测试的参与者计数错误");
        }
        kernel::test::require(state.expected_participant == 0, "FIFO 调度测试结束状态错误");

        for (auto &worker : workers) worker.reset();
    }
}  // namespace kernel::test::cases
