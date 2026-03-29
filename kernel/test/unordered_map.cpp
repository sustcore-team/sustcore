/**
 * @file unordered_map.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief unordered_map 测试
 * @version alpha-1.0.0
 * @date 2026-03-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/unordered_map.h>

#include <unordered_map>
#include <utility>

namespace test::unordered_map {

    class CaseDefaultConstruct : public TestCase {
    public:
        CaseDefaultConstruct() : TestCase("UnorderedMap 默认构造与空状态测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            expect("默认构造后容器应为空");
            ttest(map.empty());
            ttest(map.size() == 0);
            ttest(map.begin() == map.end());
        }
    };

    class CaseInsertSingle : public TestCase {
    public:
        CaseInsertSingle() : TestCase("UnorderedMap 单元素插入测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("插入单个键值对");
            auto result = map.insert(std::make_pair(7, 42));

            ttest(result.second);
            ttest(!map.empty());
            ttest(map.size() == 1);
            ttest(result.first != map.end());
            ttest(result.first->first == 7);
            ttest(result.first->second == 42);
            ttest(map.begin()->first == 7);
            ttest(map.begin()->second == 42);
        }
    };

    class CaseDuplicateInsert : public TestCase {
    public:
        CaseDuplicateInsert() : TestCase("UnorderedMap 重复键插入测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("首次插入应成功");
            auto first = map.insert(std::make_pair(3, 100));
            ttest(first.second);
            ttest(map.size() == 1);

            action("重复键再次插入应返回失败，并保留原值");
            auto second = map.insert(std::make_pair(3, 999));
            ttest(!second.second);
            ttest(map.size() == 1);
            ttest(second.first != map.end());
            ttest(second.first->first == 3);
            ttest(second.first->second == 100);
        }
    };

    class CaseIteration : public TestCase {
    public:
        CaseIteration() : TestCase("UnorderedMap 遍历与元素完整性测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("插入多个元素");
            map.insert(std::make_pair(1, 10));
            map.insert(std::make_pair(2, 20));
            map.insert(std::make_pair(3, 30));

            int key_sum   = 0;
            int value_sum = 0;
            int count     = 0;
            bool seen1    = false;
            bool seen2    = false;
            bool seen3    = false;

            check("遍历应覆盖全部已插入元素");
            for (auto it = map.begin(); it != map.end(); ++it) {
                key_sum += it->first;
                value_sum += it->second;
                count += 1;

                if (it->first == 1 && it->second == 10) {
                    seen1 = true;
                }
                if (it->first == 2 && it->second == 20) {
                    seen2 = true;
                }
                if (it->first == 3 && it->second == 30) {
                    seen3 = true;
                }
            }

            ttest(count == 3);
            ttest(key_sum == 6);
            ttest(value_sum == 60);
            ttest(seen1);
            ttest(seen2);
            ttest(seen3);
        }
    };

    class CaseMoveConstruct : public TestCase {
    public:
        CaseMoveConstruct() : TestCase("UnorderedMap 移动构造测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> source;
            source.insert(std::make_pair(11, 110));
            source.insert(std::make_pair(22, 220));

            action("移动构造新的容器");
            std::unordered_map<int, int> moved(std::move(source));

            ttest(moved.size() == 2);
            ttest(!moved.empty());
            ttest(source.size() == 0);
            ttest(source.empty());

            int count     = 0;
            int key_sum   = 0;
            int value_sum = 0;
            for (auto it = moved.begin(); it != moved.end(); ++it) {
                count += 1;
                key_sum += it->first;
                value_sum += it->second;
            }

            ttest(count == 2);
            ttest(key_sum == 33);
            ttest(value_sum == 330);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseDefaultConstruct());
        cases.push_back(new CaseInsertSingle());
        cases.push_back(new CaseDuplicateInsert());
        cases.push_back(new CaseIteration());
        cases.push_back(new CaseMoveConstruct());

        framework.add_category(new TestCategory("unordered_map",
                                                std::move(cases)));
    }
}  // namespace test::unordered_map
