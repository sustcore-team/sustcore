/**
 * @file rr.cpp
 * @brief RR 调度器 1000 时间片调度模拟测试
 */

#include <schd/rr.h>
#include <sus/units.h>
#include <test/schd/rr.h>

namespace test::schd_test::rr {

    struct SimThread {
        int id            = 0;
        int total_ticks   = 0;
        int executed      = 0;
        bool finished     = false;

        schd::rr::Entity rr_entity;
    };

    class CaseRR1000Ticks : public TestCase {
    public:
        CaseRR1000Ticks() : TestCase("RR 1000 时间片调度模拟") {
            // 调度模拟中检查次数很多, 关闭成功检查输出以减少日志
            set_verbose_pass(false);
        }

        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr int THREAD_COUNT = 4;
            constexpr int TOTAL_TICKS  = 1000;
            const int workloads[THREAD_COUNT] = {100, 200, 300, 400};

            schd::rr::RR<SimThread> scheduler;

            SimThread threads[THREAD_COUNT];
            for (int i = 0; i < THREAD_COUNT; ++i) {
                threads[i].id          = i;
                threads[i].total_ticks = workloads[i];
                threads[i].executed    = 0;
                threads[i].finished    = false;
                threads[i].rr_entity.state     = ThreadState::EMPTY;
                threads[i].rr_entity.slice_cnt = 0;
                threads[i].rr_entity.list_head.clear();
            }

            expect("将所有线程按到达顺序插入 RR 调度器");
            for (int i = 0; i < THREAD_COUNT; ++i) {
                auto ret = scheduler.insert(util::nonnull_from(threads[i]));
                tassert(ret.has_value(), "insert 返回成功");
            }

            int schedule[TOTAL_TICKS];
            SimThread* current      = nullptr;
            int total_executed      = 0;
            int last_id             = -1;
            int consecutive_run     = 0;

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

                    current          = su;
                    consecutive_run  = 0;
                    last_id          = -1;  // 下面会立即刷新
                }

                schedule[tick] = current->id;
                if (last_id == current->id) {
                    ++consecutive_run;
                } else {
                    consecutive_run = 1;
                    last_id         = current->id;
                }

                // 执行一个时间片: on_tick + 累计执行时间
                scheduler.on_tick(util::nonnull_from(*current), 1_ticks);
                current->executed++;
                total_executed++;

                // 判断是否还有其它未完成线程
                bool has_others = false;
                for (int i = 0; i < THREAD_COUNT; ++i) {
                    if (!threads[i].finished && &threads[i] != current) {
                        has_others = true;
                        break;
                    }
                }

                if (has_others) {
                    // 在存在其它就绪线程时, 任一线程连续运行的时间片数不应超过 TIME_SLICES
                    ttest(consecutive_run <= schd::rr::RR<SimThread>::TIME_SLICES);
                }

                if (current->executed >= current->total_ticks) {
                    current->finished = true;

                    // 线程完成后不再重新放入就绪队列
                    auto* e = schd::rr::RR<SimThread>::get_entity(current);
                    e->state = ThreadState::YIELD;

                    current = nullptr;
                    continue;
                }

                // 线程未完成, 检查是否时间片耗尽需要轮转
                auto runout_res = scheduler.runout(
                    util::nonnull_from(static_cast<const SimThread&>(*current)));
                tassert(runout_res.has_value(), "runout 调用成功");

                if (runout_res.value()) {
                    // 只有在存在其它就绪线程且时间片用完时才应发生
                    ttest(has_others);
                    if (has_others) {
                        ttest(consecutive_run == schd::rr::RR<SimThread>::TIME_SLICES);
                    }

                    auto* e = schd::rr::RR<SimThread>::get_entity(current);
                    e->state = ThreadState::READY;

                    auto put_res = scheduler.put(util::nonnull_from(*current));
                    tassert(put_res.has_value(), "put 重新入队成功");

                    current = nullptr;
                }
            }

            check("RR 模拟在 1000 时间片内结束且所有线程完成");
            ttest(total_executed == TOTAL_TICKS);
            for (int i = 0; i < THREAD_COUNT; ++i) {
                ttest(threads[i].finished);
                ttest(threads[i].executed == threads[i].total_ticks);
            }
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseRR1000Ticks());

        framework.add_category(new TestCategory("schd.rr", std::move(cases)));
    }

}  // namespace test::schd_test::rr
