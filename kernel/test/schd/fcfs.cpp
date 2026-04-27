/**
 * @file fcfs.cpp
 * @brief FCFS 调度器 1000 时间片调度模拟测试
 */

#include <schd/fcfs.h>
#include <test/schd/fcfs.h>

namespace test::schd_test::fcfs {
    struct TestThread {
        schd::SchedMeta meta{};
        schd::SchedMeta* basic_entity = &meta;
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
                tassert(scheduler.enqueue(util::guarantee_nonnull(&rq),
                              util::guarantee_nonnull(&first)).has_value(),
                    "第一个线程入队成功");
                tassert(scheduler.enqueue(util::guarantee_nonnull(&rq),
                              util::guarantee_nonnull(&second)).has_value(),
                    "第二个线程入队成功");
            ttest(rq.fcfs_list.size() == 2);
            ttest(first.meta.state == ThreadState::READY);
            ttest(second.meta.state == ThreadState::READY);

            action("取出队首线程");
                auto next = scheduler.pick_next(util::guarantee_nonnull(&rq));
            tassert(next.has_value(), "成功取到可运行线程");
            ttest(rq.fcfs_list.size() == 1);
            ttest(first.meta.state == ThreadState::RUNNING);

            action("将第二个线程从队列中移除");
                tassert(scheduler.dequeue(util::guarantee_nonnull(&rq),
                              util::guarantee_nonnull(&second)).has_value(),
                    "第二个线程出队成功");
            ttest(rq.fcfs_list.empty());
            ttest(second.meta.state == ThreadState::EMPTY);
        }
    };

    class CaseYieldFlag : public TestCase {
    public:
        CaseYieldFlag() : TestCase("FCFS 主动让出时设置重调度标志") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            schd::fcfs::FCFS<TestThread> scheduler;
            TestThread current{};
            scheduler.cursched = &current.meta;
            schd::RQ rq{};

            expect("当前线程调用 yield 后, 需要重调度标志应被置位");
            tassert(scheduler.yield(util::guarantee_nonnull(&rq)).has_value(), "yield 调用成功");
            ttest((current.meta.flags & schd::SchedMeta::FLAGS_NEED_RESCHED) != 0);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseEmptyQueue());
        cases.push_back(new CaseQueueOrder());
        cases.push_back(new CaseYieldFlag());
        framework.add_category(new TestCategory("schd.fcfs", std::move(cases)));
    }

}  // namespace test::schd_test::fcfs
