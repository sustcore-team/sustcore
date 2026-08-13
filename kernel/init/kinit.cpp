/**
 * @file kinit.cpp
 * @brief kinit 与多个 worker 的协作式 FIFO 调度验证
 */

#include <init/kinit.h>
#include <log.h>
#include <obj/process.h>
#include <obj/thread.h>
#include <scheduler/scheduler.h>

#include <cstddef>
#include <utility>

namespace init {
    namespace {
        constexpr size_t WORKER_COUNT      = 8;
        constexpr size_t PARTICIPANT_COUNT = WORKER_COUNT + 1;
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
        constexpr const char *WORKER_NAMES[WORKER_COUNT] = {
            "worker-1", "worker-2", "worker-3", "worker-4",
            "worker-5", "worker-6", "worker-7", "worker-8",
        };
        WorkerArgument worker_arguments[WORKER_COUNT]{};
        cap::ObjectRef<task::Thread> workers[WORKER_COUNT]{};

        auto initialized_process = task::initialize_kernel_process();
        if (!initialized_process)
            kernel::log::panic("无法初始化 kernel_process: {}",
                               tay::to_string(initialized_process.error()));
        auto kinit       = task::Thread::adopt_current(task::kernel_process());
        auto initialized = scheduler::instance().initialize(kinit);
        if (!initialized)
            kernel::log::panic("无法初始化 FIFO 调度器: {}", tay::to_string(initialized.error()));

        auto usrboot = start_usrboot();
        if (!usrboot)
            kernel::log::panic("usrboot 启动失败: {}", tay::to_string(usrboot.error()));

        for (size_t index = 0; index < WORKER_COUNT; ++index) {
            worker_arguments[index] = WorkerArgument{&state, index + 1, WORKER_NAMES[index]};
            auto worker = task::Thread::create_kernel(task::kernel_process(), worker_entry,
                                                      &worker_arguments[index]);
            if (!worker)
                kernel::log::panic("无法创建 FIFO worker {}: {}", index + 1,
                                   tay::to_string(worker.error()));
            workers[index] = std::move(*worker);
            if (auto resumed = scheduler::instance().resume(*workers[index]); !resumed)
                kernel::log::panic("无法发布 FIFO worker {}: {}", index + 1,
                                   tay::to_string(resumed.error()));
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
                kernel::log::panic("FIFO 调度测试计数错误: participant={}, completed={}",
                                   participant, state.completed[participant]);
        }
        if (state.expected_participant != 0)
            kernel::log::panic("FIFO 调度测试结束状态错误");

        for (auto &worker : workers) worker.reset();
        kernel::log::info("FIFO 调度测试通过: kinit 与八个 worker 各运行 {} 次", ITERATION_COUNT);
        kernel::log::halt();
    }
}  // namespace init
