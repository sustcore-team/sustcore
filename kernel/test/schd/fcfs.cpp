/**
 * @file fcfs.cpp
 * @brief FCFS 调度器 1000 时间片调度模拟测试
 */

#include <kio.h>
#include <schd/fcfs.h>
#include <sus/nonnull.h>
#include <sus/queue.h>
#include <sus/tostring.h>
#include <test/schd/fcfs.h>

#include <algorithm>

namespace test::schd_test::fcfs {
    struct TestThread {
        schd::SchedMeta basic_entity{};
    };

    class CaseEmptyQueue : public TestCase {
    public:
        CaseEmptyQueue() : TestCase("FCFS 空队列行为") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::fcfs::FCFS<TestThread> scheduler;
            schd::RQ rq{};

            expect("在没有就绪线程时尝试取下一个线程");
            auto next = scheduler.pick_next(util::guarantee_nonnull(&rq));
            ttest(!next.has_value());

            expect("在没有当前线程时调用 current");
            auto current = scheduler.current();
            ttest(!current.has_value());
        }
    };

    class CaseQueueOrder : public TestCase {
    public:
        CaseQueueOrder() : TestCase("FCFS 入队出队与顺序调度") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::fcfs::FCFS<TestThread> scheduler;
            schd::RQ rq{};
            TestThread first{};
            TestThread second{};

            expect("先后入队两个线程后, 队列应保持先进先出顺序");
            tassert(scheduler
                        .enqueue(util::guarantee_nonnull(&rq),
                                 util::guarantee_nonnull(&first))
                        .has_value(),
                    "第一个线程入队成功");
            tassert(scheduler
                        .enqueue(util::guarantee_nonnull(&rq),
                                 util::guarantee_nonnull(&second))
                        .has_value(),
                    "第二个线程入队成功");
            ttest(rq.fcfs_list.size() == 2);
            ttest(first.basic_entity.state == ThreadState::READY);
            ttest(second.basic_entity.state == ThreadState::READY);

            action("取出队首线程");
            auto next = scheduler.pick_next(util::guarantee_nonnull(&rq));
            tassert(next.has_value(), "成功取到可运行线程");
            ttest(rq.fcfs_list.size() == 1);
            ttest(first.basic_entity.state == ThreadState::RUNNING);

            action("将第二个线程从队列中移除");
            tassert(scheduler
                        .dequeue(util::guarantee_nonnull(&rq),
                                 util::guarantee_nonnull(&second))
                        .has_value(),
                    "第二个线程出队成功");
            ttest(rq.fcfs_list.empty());
            ttest(second.basic_entity.state == ThreadState::EMPTY);
        }
    };

    class CaseYieldFlag : public TestCase {
    public:
        CaseYieldFlag() : TestCase("FCFS 主动让出时设置重调度标志") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::fcfs::FCFS<TestThread> scheduler;
            TestThread current{};
            scheduler.cursched = &current.basic_entity;
            schd::RQ rq{};

            expect("当前线程调用 yield 后, 需要重调度标志应被置位");
            tassert(scheduler.yield(util::guarantee_nonnull(&rq)).has_value(),
                    "yield 调用成功");
            ttest((current.basic_entity.flags &
                   schd::SchedMeta::FLAGS_NEED_RESCHED) != 0);
        }
    };

    class CaseTestRunning : public TestCase {
    public:
        CaseTestRunning() : TestCase("FCFS模拟运行") {}
        struct SpecialThread {
            size_t id;            // 线程ID
            size_t maxcnt;        // 线程最大运行时间
            size_t cnt;           // 线程已运行时间
            size_t enqueue_time;  // 入队时间
            size_t yield_cnt;    // 让出时间
            schd::SchedMeta basic_entity{};
        };

        constexpr static size_t NUM_THREADS         = 100;
        constexpr static size_t SYSTEM_RUNNING_TIME = 2000;
        constexpr static size_t MAX_TIME            = 20;
        constexpr static size_t MIN_TIME            = 5;
        // enqueue a thread every ENQUEUE_GAP ticks
        constexpr static size_t ENQUEUE_GAP         = 4;

        mutable SpecialThread threads[NUM_THREADS];
        mutable size_t RECORD[SYSTEM_RUNNING_TIME]          = {};

        void init_threads() const {
            for (size_t i = 0; i < NUM_THREADS; ++i) {
                threads[i].id     = i + 1;
                threads[i].maxcnt = std::clamp((i + 1) * 2, MIN_TIME, MAX_TIME);
                threads[i].cnt    = 0;
                threads[i].yield_cnt = threads[i].maxcnt / 2;  // 在运行到一半时让出一次
                threads[i].enqueue_time = i * ENQUEUE_GAP;
            }
        }

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::fcfs::FCFS<SpecialThread> scheduler;
            schd::RQ rq{};

            init_threads();

            bool exit_normally = true;
            size_t time = 0;

            for (time = 0; time < SYSTEM_RUNNING_TIME; time++) {
                // foreach the threads and enqueue those who should be enqueued
                // at this time
                for (size_t i = 0; i < NUM_THREADS; ++i) {
                    if (threads[i].enqueue_time == time) {
                        auto enqueue_res = scheduler.enqueue(
                            util::guarantee_nonnull(&rq),
                            util::guarantee_nonnull(&threads[i]));
                        if (!enqueue_res.has_value()) {
                            exit_normally = false;
                            break;
                        }
                    }
                }
                if (!exit_normally) {
                    break;
                }

                auto current_opt = scheduler.current();

                RECORD[time] =
                    current_opt.has_value() ? current_opt.value()->id : 0;

                bool exit_flag = false;

                if (current_opt.has_value()) {
                    auto current = current_opt.value();
                    current->cnt++;

                    // if the thread has run for its required time, dequeue it
                    if (current->cnt >= current->maxcnt) {
                        exit_flag = true;
                        auto yield_res = scheduler.yield(util::nonnull_from(rq));
                        if (!yield_res.has_value()) {
                            exit_normally = false;
                            break;
                        }
                    }

                    if (current->yield_cnt == current->cnt) {
                        auto yield_res = scheduler.yield(util::nonnull_from(rq));
                        if (!yield_res.has_value()) {
                            exit_normally = false;
                            break;
                        }
                    }
                }

                if (current_opt.has_value()) {
                    auto current = current_opt.value();
                    scheduler.on_tick(util::nonnull_from(rq), current);
                }

                bool reschedflag = false;
                if (! current_opt.has_value()) {
                    reschedflag = true;
                } else {
                    reschedflag = (current_opt.value()->basic_entity.flags &
                                  schd::SchedMeta::FLAGS_NEED_RESCHED) != 0;
                }

                if (reschedflag) {
                    if (current_opt.has_value() && !exit_flag) {
                        auto current = current_opt.value();
                        auto put_prev_res =
                            scheduler.put_prev(util::nonnull_from(rq), current);
                        current->basic_entity.flags_reset<schd::SchedMeta::FLAGS_NEED_RESCHED>();
                        if (!put_prev_res.has_value()) {
                            exit_normally = false;
                            break;
                        }
                    }

                    auto pick_next_res = scheduler.pick_next(util::nonnull_from(rq));
                    if (!pick_next_res.has_value()) {
                        // end the simulation
                        break;
                    }
                }
            }

            if (!exit_normally) {
                loggers::SUSTCORE::ERROR("模拟运行过程中发生错误! 时间: %d", time);
            }
            tassert(exit_normally, "模拟运行过程中发生错误");

            // 比较实际记录与预期记录
            for (size_t time = 0; time < SYSTEM_RUNNING_TIME; ++time) {
                loggers::SUSTCORE::DEBUG("Time %d: Running Thread ID = %d", time, RECORD[time]);
            }
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseEmptyQueue());
        cases.push_back(new CaseQueueOrder());
        cases.push_back(new CaseYieldFlag());
        cases.push_back(new CaseTestRunning());
        framework.add_category(new TestCategory("schd.fcfs", std::move(cases)));
    }

}  // namespace test::schd_test::fcfs
