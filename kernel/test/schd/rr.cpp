/**
 * @file rr.cpp
 * @brief RR 调度器 1000 时间片调度模拟测试
 */

#include <schd/rr.h>
#include <sus/units.h>
#include <test/schd/rr.h>

namespace test::schd_test::rr {
    struct TestThread {
        schd::SchedMeta basic_entity{};
        schd::rr::Entity rr_entity{};
    };

    class CaseEmptyQueue : public TestCase {
    public:
        CaseEmptyQueue() : TestCase("RR 空队列行为") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::rr::RR<TestThread> scheduler;
            schd::RQ rq{};

            expect("在没有就绪线程时尝试取下一个线程");
            auto next = scheduler.pick_next(util::nnullforce(&rq));
            ttest(!next.has_value());
        }
    };

    class CaseTimeSlice : public TestCase {
    public:
        CaseTimeSlice() : TestCase("RR 时间片递减与耗尽") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::rr::RR<TestThread> scheduler;
            TestThread first{};

            expect("直接对运行中的线程执行 on_tick 时应递减时间片");
            first.rr_entity.slice_cnt = schd::rr::RR<TestThread>::TIME_SLICES;
            schd::RQ rq{};

            action("连续消耗一个线程的时间片");
            for (int i = 0; i < schd::rr::RR<TestThread>::TIME_SLICES - 1; ++i) {
                tassert(scheduler.on_tick(util::nnullforce(&rq),
                                          util::nnullforce(&first)).has_value(),
                        "时间片递减调用成功");
                ttest(first.rr_entity.slice_cnt == schd::rr::RR<TestThread>::TIME_SLICES - 1 - i);
                ttest((first.basic_entity.flags & schd::SchedMeta::FLAGS_NEED_RESCHED) == 0);
            }

            tassert(scheduler.on_tick(util::nnullforce(&rq),
                                      util::nnullforce(&first)).has_value(),
                    "最后一次时间片递减成功");
            ttest(first.rr_entity.slice_cnt == 0);
            ttest((first.basic_entity.flags & schd::SchedMeta::FLAGS_NEED_RESCHED) != 0);
        }
    };

    class CaseQueueOps : public TestCase {
    public:
        CaseQueueOps() : TestCase("RR 入队与出队") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::rr::RR<TestThread> scheduler;
            schd::RQ rq{};
            TestThread first{};
            TestThread second{};

            tassert(scheduler.enqueue(util::nnullforce(&rq),
                                      util::nnullforce(&first)).has_value(),
                    "第一个线程入队成功");
            tassert(scheduler.enqueue(util::nnullforce(&rq),
                                      util::nnullforce(&second)).has_value(),
                    "第二个线程入队成功");

            ttest(rq.rr_list.size() == 2);

            tassert(scheduler.dequeue(util::nnullforce(&rq),
                                      util::nnullforce(&second)).has_value(),
                    "第二个线程出队成功");
            ttest(rq.rr_list.size() == 1);
            ttest(second.basic_entity.state == ThreadState::EMPTY);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseEmptyQueue());
        cases.push_back(new CaseTimeSlice());
        cases.push_back(new CaseQueueOps());
        framework.add_category(new TestCategory("schd.rr", std::move(cases)));
    }

}  // namespace test::schd_test::rr
