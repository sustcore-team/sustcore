/**
 * @file unordered_set.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief unordered_set 测试
 * @version alpha-1.0.0
 * @date 2026-06-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/unordered_set.h>

#include <unordered_set>

namespace test::unordered_set {

    struct ConstantHash {
        size_t operator()(int key [[maybe_unused]]) const noexcept {
            return 0;
        }
    };

    class CaseDefaultConstruct : public TestCase {
    public:
        CaseDefaultConstruct() : TestCase("UnorderedSet 默认构造与空状态测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set;

            expect("默认构造后容器应为空");
            ttest(set.empty());
            ttest(set.size() == 0);
            ttest(set.begin() == set.end());
        }
    };

    class CaseInsertSingle : public TestCase {
    public:
        CaseInsertSingle() : TestCase("UnorderedSet 单元素插入测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set;

            action("插入单个元素");
            auto result = set.insert(7);

            ttest(result.second);
            ttest(!set.empty());
            ttest(set.size() == 1);
            ttest(result.first != set.end());
            ttest(*result.first == 7);
            ttest(*set.begin() == 7);
        }
    };

    class CaseDuplicateInsert : public TestCase {
    public:
        CaseDuplicateInsert() : TestCase("UnorderedSet 重复元素插入测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set;

            action("首次插入应成功");
            auto first = set.insert(3);
            ttest(first.second);
            ttest(set.size() == 1);

            action("重复元素再次插入应返回失败");
            auto second = set.insert(3);
            ttest(!second.second);
            ttest(set.size() == 1);
            ttest(second.first != set.end());
            ttest(*second.first == 3);
        }
    };

    class CaseCrudFlow : public TestCase {
    public:
        CaseCrudFlow() : TestCase("UnorderedSet 增删查完整流程测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set;

            action("Create: 插入基础数据");
            auto created = set.insert(10);
            ttest(created.second);
            ttest(set.size() == 1);

            check("Read: find/contains 应能找到已存在元素");
            auto found = set.find(10);
            ttest(found != set.end());
            ttest(*found == 10);
            ttest(set.contains(10));
            ttest(!set.contains(99));
            ttest(set.find(99) == set.end());
            ttest(set.count(10) == 1);
            ttest(set.count(99) == 0);

            action("Delete: erase(key) 应删除目标元素");
            ttest(set.erase(10) == 1);
            ttest(set.size() == 0);
            ttest(set.empty());
            ttest(!set.contains(10));
            ttest(set.find(10) == set.end());

            action("删除不存在元素应返回 0");
            ttest(set.erase(10) == 0);
        }
    };

    class CaseIteration : public TestCase {
    public:
        CaseIteration() : TestCase("UnorderedSet 遍历与元素完整性测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set;

            action("插入多个元素");
            set.insert(1);
            set.insert(2);
            set.insert(3);

            int sum   = 0;
            int count = 0;
            bool seen1 = false;
            bool seen2 = false;
            bool seen3 = false;

            check("遍历应覆盖全部已插入元素");
            for (auto it = set.begin(); it != set.end(); ++it) {
                sum += *it;
                count += 1;

                if (*it == 1) {
                    seen1 = true;
                }
                if (*it == 2) {
                    seen2 = true;
                }
                if (*it == 3) {
                    seen3 = true;
                }
            }

            ttest(count == 3);
            ttest(sum == 6);
            ttest(seen1);
            ttest(seen2);
            ttest(seen3);
        }
    };

    class CaseCollisionHandling : public TestCase {
    public:
        CaseCollisionHandling() : TestCase("UnorderedSet 冲突链读写删除测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int, ConstantHash> set(4);

            action("构造强制哈希冲突的数据集");
            set.insert(1);
            set.insert(2);
            set.insert(3);

            check("冲突链中每个元素都应可读");
            ttest(set.find(1) != set.end());
            ttest(set.find(2) != set.end());
            ttest(set.find(3) != set.end());

            action("删除链中间元素后, 其余元素仍应可达");
            ttest(set.erase(2) == 1);
            ttest(set.size() == 2);
            ttest(set.find(2) == set.end());
            ttest(set.find(1) != set.end());
            ttest(set.find(3) != set.end());
        }
    };

    class CaseBulkCrud : public TestCase {
    public:
        CaseBulkCrud() : TestCase("UnorderedSet 批量 CRUD 回归测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set;

            action("批量插入 64 个元素, 覆盖 rehash 路径");
            for (int i = 0; i < 64; ++i) {
                auto result = set.insert(i);
                ttest(result.second);
            }
            ttest(set.size() == 64);

            check("批量读取校验");
            for (int i = 0; i < 64; ++i) {
                ttest(set.find(i) != set.end());
            }

            action("批量删除部分元素");
            int erased = 0;
            for (int i = 0; i < 64; i += 4) {
                erased += static_cast<int>(set.erase(i));
            }
            ttest(erased == 16);
            ttest(set.size() == 48);

            check("删除后剩余元素应保持可读");
            for (int i = 0; i < 64; ++i) {
                auto it = set.find(i);
                if (i % 4 == 0) {
                    ttest(it == set.end());
                } else {
                    ttest(it != set.end());
                }
            }

            action("clear 后应恢复为空容器");
            set.clear();
            ttest(set.empty());
            ttest(set.size() == 0);
            ttest(set.begin() == set.end());
        }
    };

    class CaseMoveConstruct : public TestCase {
    public:
        CaseMoveConstruct() : TestCase("UnorderedSet 移动构造测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> source;
            source.insert(11);
            source.insert(22);

            action("移动构造新的容器");
            std::unordered_set<int> moved(std::move(source));

            ttest(moved.size() == 2);
            ttest(!moved.empty());
            ttest(source.size() == 0);
            ttest(source.empty());

            int count = 0;
            int sum   = 0;
            for (auto it = moved.begin(); it != moved.end(); ++it) {
                count += 1;
                sum += *it;
            }

            ttest(count == 2);
            ttest(sum == 33);
        }
    };

    class CaseStandardInterfaceSubset : public TestCase {
    public:
        CaseStandardInterfaceSubset()
            : TestCase("UnorderedSet 常用标准接口测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_set<int> set{1, 2};

            ttest(set.size() == 2);
            ttest(set.count(1) == 1);
            ttest(set.count(99) == 0);

            auto emplaced = set.emplace(3);
            ttest(emplaced.second);
            ttest(set.find(3) != set.end());

            auto duplicate = set.emplace(3);
            ttest(!duplicate.second);
            ttest(set.size() == 3);

            auto range = set.equal_range(2);
            ttest(range.first != set.end());
            ttest(*range.first == 2);
            ttest(range.second != range.first);

            size_t old_bucket_count = set.bucket_count();
            ttest(old_bucket_count >= 1);
            ttest(set.bucket_size(set.bucket(1)) >= 1);

            set.reserve(32);
            ttest(set.bucket_count() >= old_bucket_count);

            std::unordered_set<int> copy = set;
            ttest(copy.size() == set.size());
            ttest(copy.find(3) != copy.end());

            std::unordered_set<int> assigned_set;
            assigned_set = {7, 8};
            ttest(assigned_set.size() == 2);
            ttest(assigned_set.find(8) != assigned_set.end());

            assigned_set.swap(copy);
            ttest(assigned_set.find(3) != assigned_set.end());
            ttest(copy.find(7) != copy.end());

            auto erase_begin = assigned_set.begin();
            assigned_set.erase(erase_begin);
            ttest(assigned_set.size() == set.size() - 1);

            assigned_set.erase(assigned_set.begin(), assigned_set.end());
            ttest(assigned_set.empty());
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseDefaultConstruct());
        cases.push_back(new CaseInsertSingle());
        cases.push_back(new CaseDuplicateInsert());
        cases.push_back(new CaseCrudFlow());
        cases.push_back(new CaseIteration());
        cases.push_back(new CaseCollisionHandling());
        cases.push_back(new CaseBulkCrud());
        cases.push_back(new CaseMoveConstruct());
        cases.push_back(new CaseStandardInterfaceSubset());

        framework.add_category(new TestCategory("unordered_set",
                                                std::move(cases)));
    }
}  // namespace test::unordered_set
