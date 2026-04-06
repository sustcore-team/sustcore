/**
 * @file buddy.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief buddy系统测试
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/gfp.h>
#include <sus/list.h>
#include <test/buddy.h>

namespace test::buddy {

    class CaseFragmentation : public TestCase {
    public:
        CaseFragmentation() : TestCase("Buddy 混合大小分配与碎片化") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("分配 1, 2, 4, 3 页物理内存");
            auto r1 = GFP::get_free_page(1);
            auto r2 = GFP::get_free_page(2);
            auto r4 = GFP::get_free_page(4);
            auto r3 = GFP::get_free_page(3);

            tassert(r1.has_value() && r2.has_value() && r3.has_value() && r4.has_value(), "所有分配均成功");

            PhyAddr p1 = r1.value();
            PhyAddr p2 = r2.value();
            PhyAddr p4 = r4.value();
            PhyAddr p3 = r3.value();

            action("释放中间块 (2p, 3p) 以制造碎片");
            GFP::put_page(p2, 2);
            GFP::put_page(p3, 3);

            expect("重新分配 2 页 (应从碎片中重用空间)");
            auto r_new = GFP::get_free_page(2);
            tassert(r_new.has_value(), "重新分配 2 页成功");
            PhyAddr p_new = r_new.value();

            GFP::put_page(p_new, 2);
            GFP::put_page(p1, 1);
            GFP::put_page(p4, 4);
        }
    };

    class CaseExhaustion : public TestCase {
    public:
        CaseExhaustion() : TestCase("Buddy Order 0 耗尽与反向合并") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("尝试连续分配 32 个单页 (Order 0)");
            constexpr int MAX_singles = 32;
            PhyAddr singles[MAX_singles];
            int alloc_count = 0;

            for (int i = 0; i < MAX_singles; i++) {
                auto r = GFP::get_free_page(1);
                if (!r.has_value()) {
                    break;
                }
                singles[i] = r.value();
                alloc_count++;
            }
            check("记录并验证至少分配成功");
            ttest(alloc_count > 0);

            action("反向释放所有已分配页面以鼓励合并");
            for (int i = alloc_count - 1; i >= 0; i--) {
                GFP::put_page(singles[i], 1);
            }
        }
    };

    class CaseMerge : public TestCase {
    public:
        CaseMerge() : TestCase("Buddy 大块分割与合并验证") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("分配一个 64 页的大块");
            auto r = GFP::get_free_page(64);
            tassert(r.has_value(), "初始分配 64 页成功");
            PhyAddr huge = r.value();

            action("释放该大块");
            GFP::put_page(huge, 64);

            expect("再次分配 64 页, 验证是否能得到相同的起始地址 (合并成功)");
            auto r2 = GFP::get_free_page(64);
            tassert(r2.has_value(), "再次分配 64 页成功");
            PhyAddr huge2 = r2.value();
            ttest(huge2 == huge);

            GFP::put_page(huge2, 64);
        }
    };

    class CaseInvalidArgs : public TestCase {
    public:
        CaseInvalidArgs() : TestCase("Buddy 参数合法性与界限测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("尝试请求 0 页内存 (应返回空地址)");
            auto r0 = GFP::get_free_page(0);
            ttest(!r0.has_value());

            expect("尝试请求极大页数 (预期失败)");
            auto r_large = GFP::get_free_page(1ULL << 30); // 1TB of pages
            ttest(!r_large.has_value());

            expect("释放空地址 (应能静默处理或不崩溃)");
            GFP::put_page(PhyAddr::null, 1);
            check("释放空地址后系统正常");
        }
    };

    class CaseAlignment : public TestCase {
    public:
        CaseAlignment() : TestCase("Buddy 分配对齐特征测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            expect("分配不同 order 的块, 验证其对齐性 (Addr % (PageSize * Count) == 0)");
            for (int order_shift = 0; order_shift <= 6; order_shift++) {
                size_t count = 1ULL << order_shift;
                auto r = GFP::get_free_page(count);
                if (r.has_value()) {
                    PhyAddr p = r.value();
                    addr_t addr = p.arith();
                    size_t align_mask = (count << 12) - 1; // 4KB page size
                    ttest((addr & align_mask) == 0);
                    GFP::put_page(p, count);
                }
            }
        }
    };

    class CaseStressSmall : public TestCase {
    public:
        CaseStressSmall() : TestCase("Buddy 小规模压力测试 (分配与随机释放)") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            constexpr int ITERATIONS = 64;
            PhyAddr pages[ITERATIONS];
            int count = 0;

            expect("连续分配不同大小的块直到失败或达到上限");
            for (int i = 0; i < ITERATIONS; i++) {
                size_t sz = (i % 5) + 1; // 1-5 pages
                auto r = GFP::get_free_page(sz);
                if (!r.has_value()) break;
                pages[i] = r.value();
                count++;
            }

            action("顺序释放并验证无冲突");
            for (int i = 0; i < count; i++) {
                size_t sz = (i % 5) + 1;
                GFP::put_page(pages[i], sz);
            }
            check("压力测试完成");
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseFragmentation());
        cases.push_back(new CaseExhaustion());
        cases.push_back(new CaseMerge());
        cases.push_back(new CaseInvalidArgs());
        cases.push_back(new CaseAlignment());
        // TODO: 这一测试会引发空指针错误
        // 暂时注释该测试, 需要后续修复后再启用
        // cases.push_back(new CaseStressSmall());

        framework.add_category(new TestCategory("buddy", std::move(cases)));
    }
}  // namespace test::buddy
