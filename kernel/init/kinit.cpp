/**
 * @file kinit.cpp
 * @brief kinit 与双 worker 的协作式 FIFO 调度验证
 */

#include <init/kinit.h>
#include <log.h>
#include <scheduler/scheduler.h>
#include <task/thread.h>

#include <cstddef>

namespace init {
    namespace {
        constexpr size_t PARTICIPANT_COUNT = 3;
        constexpr size_t ITERATION_COUNT   = 10;

        struct TestState final {
            size_t expected_participant = 0;
            size_t completed[PARTICIPANT_COUNT]{};
        };

        struct WorkerArgument final {
            TestState *state = nullptr;
            size_t participant;
            const char *name = nullptr;
        };

        void run_participant(TestState &state, size_t participant, const char *name) noexcept {
            for (size_t iteration = 0; iteration < ITERATION_COUNT; ++iteration) {
                if (state.expected_participant != participant ||
                    state.completed[participant] != iteration)
                {
                    kernel::log::panic("FIFO 调度次序错误: 期望参与者={}, 实际参与者={}, 轮次={}",
                                       state.expected_participant, participant, iteration + 1);
                }
                kernel::log::info("FIFO 调度测试: {} {}/{}", name, iteration + 1, ITERATION_COUNT);
                ++state.completed[participant];
                state.expected_participant = (participant + 1) % PARTICIPANT_COUNT;
                scheduler::yield();
            }
        }

        void worker_entry(void *opaque) noexcept {
            auto *argument = static_cast<WorkerArgument *>(opaque);
            if (argument == nullptr || argument->state == nullptr || argument->name == nullptr)
                kernel::log::panic("无效的 FIFO worker 参数");
            run_participant(*argument->state, argument->participant, argument->name);
        }
    }  // namespace

    [[noreturn]] void run_kinit() noexcept {
        TestState state{};
        WorkerArgument worker_arguments[2]{
            WorkerArgument{&state, 1, "worker-1"},
            WorkerArgument{&state, 2, "worker-2"},
        };

        auto kinit       = task::Thread::adopt_current();
        auto initialized = scheduler::instance().initialize(kinit);
        if (!initialized)
            kernel::log::panic("无法初始化 FIFO 调度器: {}", tay::to_string(initialized.error()));

        auto worker1 = task::Thread::create_kernel(worker_entry, &worker_arguments[0]);
        auto worker2 = task::Thread::create_kernel(worker_entry, &worker_arguments[1]);
        if (!worker1 || !worker2)
            kernel::log::panic("无法创建 FIFO worker: worker-1={}, worker-2={}",
                               worker1 ? "OK" : tay::to_string(worker1.error()),
                               worker2 ? "OK" : tay::to_string(worker2.error()));

        auto resumed1 = scheduler::instance().resume(**worker1);
        auto resumed2 = scheduler::instance().resume(**worker2);
        if (!resumed1 || !resumed2)
            kernel::log::panic("无法发布 FIFO worker");

        run_participant(state, 0, "kinit");

        while (!(*worker1)->exited() || !(*worker2)->exited()) scheduler::yield();

        for (size_t participant = 0; participant < PARTICIPANT_COUNT; ++participant) {
            if (state.completed[participant] != ITERATION_COUNT)
                kernel::log::panic("FIFO 调度测试计数错误: participant={}, completed={}",
                                   participant, state.completed[participant]);
        }
        if (state.expected_participant != 0)
            kernel::log::panic("FIFO 调度测试结束状态错误");

        worker1->reset();
        worker2->reset();
        kernel::log::info("FIFO 调度测试通过: kinit 与两个 worker 各运行 {} 次", ITERATION_COUNT);
        kernel::log::halt();
    }
}  // namespace init
