/**
 * @file slub.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief slub系统测试
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/alloc.h>
#include <sus/list.h>
#include <test/slub.h>

namespace test::slub {

    struct SlubSmallObj {
        uint32_t a;
        uint64_t b;
        char pad[24];
    };

    struct SlubHugeObj {
        char data[3000];
    };

    class CaseSmallObjAlloc : public TestCase {
    public:
        CaseSmallObjAlloc() : TestCase("SLUB 小对象分配与释放") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            ::slub::SlubAllocator<SlubSmallObj> alloc;
            constexpr int kSmallCount             = 16;
            SlubSmallObj* small_objs[kSmallCount] = {nullptr};

            expect("连续分配 16 个小对象并写入数据");
            for (int i = 0; i < kSmallCount; i++) {
                small_objs[i] = reinterpret_cast<SlubSmallObj*>(alloc.alloc());
                if (test(small_objs[i] != nullptr, "分配成功")) {
                    small_objs[i]->a = static_cast<uint32_t>(0x1000 + i);
                    small_objs[i]->b = static_cast<uint64_t>(0x20000000ull + i);
                }
            }

            action("释放所有已分配的小对象");
            for (int i = 0; i < kSmallCount; i++) {
                if (small_objs[i]) {
                    alloc.free(small_objs[i]);
                }
            }

            check("验证统计信息：inuse 应归零");
            auto stats = alloc.get_stats();
            ttest(stats.objects_inuse == 0);
        }
    };

    class CaseObjectReuse : public TestCase {
    public:
        CaseObjectReuse() : TestCase("SLUB 对象重用趋势测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            ::slub::SlubAllocator<SlubSmallObj> alloc;
            
            expect("分配并立即释放一个对象");
            SlubSmallObj* p1 = alloc.alloc();
            tassert(p1 != nullptr);
            alloc.free(p1);

            expect("再次分配对象，验证是否重用 (理想情况下 p1 == p2)");
            SlubSmallObj* p2 = alloc.alloc();
            ttest(p2 == p1);

            if (p2) alloc.free(p2);
        }
    };

    class CaseHugeObjPath : public TestCase {
    public:
        CaseHugeObjPath() : TestCase("SLUB 大对象路径 (Huge Page) 测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            ::slub::SlubAllocator<SlubHugeObj> huge_alloc;
            
            expect("分配两个跨页的大对象");
            SlubHugeObj* h1 = huge_alloc.alloc();
            SlubHugeObj* h2 = huge_alloc.alloc();
            
            ttest(h1 != nullptr && h2 != nullptr);
            ttest(h1 != h2);

            action("释放大对象");
            if (h1) huge_alloc.free(h1);
            if (h2) huge_alloc.free(h2);

            action("测试释放 nullptr (不应崩溃)");
            huge_alloc.free(nullptr);
            check("正常退出");
        }
    };

    class CaseMultiSlab : public TestCase {
    public:
        CaseMultiSlab() : TestCase("SLUB 多 Slab 页面扩张测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            ::slub::SlubAllocator<SlubSmallObj> alloc;
            
            // 假设一个小对象 40 字节，4KB 页面大约容纳 100 个
            // 分配 150 个强制触发第二个 Slab 页面的创建
            constexpr int kCount = 150;
            SlubSmallObj* objs[kCount] = {nullptr};

            expect("分配 150 个小对象以触发 Slab 扩张");
            for (int i = 0; i < kCount; i++) {
                objs[i] = reinterpret_cast<SlubSmallObj*>(alloc.alloc());
                if (!test(objs[i] != nullptr, "分配成功")) break;
            }

            auto stats = alloc.get_stats();
            check("查看统计信息：Slabs 数量应大于 1");
            ttest(stats.total_slabs > 1);

            action("释放所有对象并检查 Slab 是否维持或正确回收");
            for (int i = 0; i < kCount; i++) {
                if (objs[i]) alloc.free(objs[i]);
            }
            
            auto stats_post = alloc.get_stats();
            ttest(stats_post.objects_inuse == 0);
        }
    };

    class CaseStressFreelist : public TestCase {
    public:
        CaseStressFreelist() : TestCase("SLUB 空闲列表一致性压力测试") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            ::slub::SlubAllocator<SlubSmallObj> alloc;
            constexpr int kIter = 100;
            SlubSmallObj* pool[kIter] = {nullptr};

            expect("执行随机顺序的分配与释放以打乱空闲列表");
            // 第一阶段：铺满
            for (int i = 0; i < kIter; i++) pool[i] = alloc.alloc();

            // 第二阶段：部分释放（偶数下标）
            for (int i = 0; i < kIter; i += 2) {
                if (pool[i]) {
                    alloc.free(pool[i]);
                    pool[i] = nullptr;
                }
            }

            // 第三阶段：再次填满
            for (int i = 0; i < kIter; i += 2) {
                pool[i] = alloc.alloc();
                ttest(pool[i] != nullptr);
            }

            action("清理所有对象");
            for (int i = 0; i < kIter; i++) {
                if (pool[i]) alloc.free(pool[i]);
            }
            check("压力测试完成，系统稳定");
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseSmallObjAlloc());
        cases.push_back(new CaseObjectReuse());
        cases.push_back(new CaseHugeObjPath());
        cases.push_back(new CaseMultiSlab());
        cases.push_back(new CaseStressFreelist());

        framework.add_category(new TestCategory("slub", std::move(cases)));
    }
}  // namespace test::slub