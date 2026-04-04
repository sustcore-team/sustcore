/**
 * @file fcfs.cpp
 * @brief FCFS 调度器 1000 时间片调度模拟测试
 */

#include <schd/fcfs.h>
#include <test/schd/fcfs.h>

namespace test::schd_test::fcfs {

    struct SimThread {
        int id            = 0;
        int total_ticks   = 0;
        int executed      = 0;
        bool finished     = false;

        schd::fcfs::Entity fcfs_entity;
    };

    class CaseFCFS1000Ticks : public TestCase {
    public:
        CaseFCFS1000Ticks() : TestCase("FCFS 1000 时间片调度模拟") {
            // 调度模拟中检查次数很多, 关闭成功检查输出以减少日志
            set_verbose_pass(false);
        }

        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr int THREAD_COUNT = 4;
            constexpr int TOTAL_TICKS  = 1000;
            const int workloads[THREAD_COUNT] = {100, 200, 300, 400};

            schd::fcfs::FCFS<SimThread> scheduler;

            SimThread threads[THREAD_COUNT];
            for (int i = 0; i < THREAD_COUNT; ++i) {
                threads[i].id          = i;
                threads[i].total_ticks = workloads[i];
                threads[i].executed    = 0;
                threads[i].finished    = false;
                threads[i].fcfs_entity.state = ThreadState::EMPTY;
                threads[i].fcfs_entity.list_head.clear();
            }

            expect("将所有线程按到达顺序插入 FCFS 调度器");
            for (int i = 0; i < THREAD_COUNT; ++i) {
                auto ret = scheduler.insert(util::nonnull_from(threads[i]));
                tassert(ret.has_value(), "insert 返回成功");
            }

            int schedule[TOTAL_TICKS];
            SimThread* current      = nullptr;
            int total_executed      = 0;

            for (int tick = 0; tick < TOTAL_TICKS; ++tick) {
                if (current == nullptr) {
                    auto pick_res = scheduler.pick();
                    tassert(pick_res.has_value(), "pick 在存在可运行线程时成功");
                    auto su_nonnull = pick_res.value();
                    SimThread* su   = su_nonnull.get();

                    auto fetch_res = scheduler.fetch(util::nonnull_from(*su));
                    tassert(fetch_res.has_value(), "fetch 成功");

                    auto set_res = scheduler.set_run(util::nonnull_from(*su));
                    tassert(set_res.has_value(), "set_run 成功");

                    current = su;
                }

                int remaining_threads = 0;
                for (int i = 0; i < THREAD_COUNT; ++i) {
                    if (!threads[i].finished && &threads[i] != current) {
                        remaining_threads++;
                    }
                }

                if (!current->finished) {
                    auto runout_res = scheduler.runout(
                        util::nonnull_from(static_cast<const SimThread&>(*current)));
                    tassert(runout_res.has_value(), "runout 调用成功");

                    if (remaining_threads > 0) {
                        // FCFS: 只要当前仍处于 RUNNING, 且有其他 READY 线程, 就不应主动切换
                        ttest(!runout_res.value());
                    }
                }

                // 执行一个时间片
                schedule[tick] = current->id;
                current->executed++;
                total_executed++;

                if (current->executed >= current->total_ticks) {
                    current->finished = true;

                    // 对于非最后一个线程, ready_queue 仍非空, 将状态置为 YIELD 后应触发 runout
                    int unfinished_others = 0;
                    for (int i = 0; i < THREAD_COUNT; ++i) {
                        if (!threads[i].finished && &threads[i] != current) {
                            unfinished_others++;
                        }
                    }

                    if (unfinished_others > 0) {
                        auto* e = schd::fcfs::FCFS<SimThread>::get_entity(current);
                        e->state = ThreadState::YIELD;

                        auto runout2 = scheduler.runout(
                            util::nonnull_from(static_cast<const SimThread&>(*current)));
                        tassert(runout2.has_value(), "runout (YIELD) 调用成功");
                        ttest(runout2.value());
                    }

                    current = nullptr;
                }
            }

            check("所有线程都在 1000 时间片内正好完成");
            ttest(total_executed == TOTAL_TICKS);
            for (int i = 0; i < THREAD_COUNT; ++i) {
                ttest(threads[i].finished);
                ttest(threads[i].executed == threads[i].total_ticks);
            }

            expect("调度顺序应严格符合 FCFS 先来先服务");
            for (int t = 0; t < TOTAL_TICKS; ++t) {
                int expected_id;
                if (t < 100) {
                    expected_id = 0;
                } else if (t < 300) {
                    expected_id = 1;
                } else if (t < 600) {
                    expected_id = 2;
                } else {
                    expected_id = 3;
                }

                if (!ttest(schedule[t] == expected_id)) {
                    break;
                }
            }
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseFCFS1000Ticks());

        framework.add_category(new TestCategory("schd.fcfs", std::move(cases)));
    }

}  // namespace test::schd_test::fcfs
