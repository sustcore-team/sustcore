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

    struct ConstantHash {
        size_t operator()(int key [[maybe_unused]]) const noexcept {
            return 0;
        }
    };

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

    class CaseCrudFlow : public TestCase {
    public:
        CaseCrudFlow() : TestCase("UnorderedMap 增删查改完整流程测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("Create: 插入基础数据");
            auto created = map.insert(std::make_pair(10, 100));
            ttest(created.second);
            ttest(map.size() == 1);

            check("Read: find/contains 应能找到已存在键");
            auto found = map.find(10);
            ttest(found != map.end());
            ttest(found->first == 10);
            ttest(found->second == 100);
            ttest(map.contains(10));
            ttest(!map.contains(99));
            ttest(map.find(99) == map.end());

            action("Update: insert_or_assign 应覆盖原值");
            auto updated = map.insert_or_assign(10, 555);
            ttest(!updated.second);
            ttest(updated.first != map.end());
            ttest(updated.first->second == 555);
            ttest(map.find(10)->second == 555);
            ttest(map.size() == 1);

            action("Delete: erase(key) 应删除目标元素");
            ttest(map.erase(10) == 1);
            ttest(map.size() == 0);
            ttest(map.empty());
            ttest(!map.contains(10));
            ttest(map.find(10) == map.end());

            action("删除不存在键应返回 0");
            ttest(map.erase(10) == 0);
        }
    };

    class CaseOperatorIndex : public TestCase {
    public:
        CaseOperatorIndex() : TestCase("UnorderedMap operator[] 默认插入与更新测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("读取不存在键应默认插入 mapped_type()");
            int& value = map[5];
            ttest(map.size() == 1);
            ttest(value == 0);
            ttest(map.find(5) != map.end());
            ttest(map.find(5)->second == 0);

            action("通过 operator[] 更新已存在键");
            map[5] = 1234;
            ttest(map.size() == 1);
            ttest(map.find(5)->second == 1234);

            action("再次访问新键应新增元素");
            map[8] = 88;
            ttest(map.size() == 2);
            ttest(map.find(8) != map.end());
            ttest(map.find(8)->second == 88);
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

    class CaseCollisionHandling : public TestCase {
    public:
        CaseCollisionHandling() : TestCase("UnorderedMap 冲突链读写删除测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int, ConstantHash> map(4);

            action("构造强制哈希冲突的数据集");
            map.insert(std::make_pair(1, 10));
            map.insert(std::make_pair(2, 20));
            map.insert(std::make_pair(3, 30));

            check("冲突链中每个元素都应可读");
            ttest(map.find(1) != map.end());
            ttest(map.find(2) != map.end());
            ttest(map.find(3) != map.end());
            ttest(map.find(1)->second == 10);
            ttest(map.find(2)->second == 20);
            ttest(map.find(3)->second == 30);

            action("删除链中间元素后，其余元素仍应可达");
            ttest(map.erase(2) == 1);
            ttest(map.size() == 2);
            ttest(map.find(2) == map.end());
            ttest(map.find(1) != map.end());
            ttest(map.find(3) != map.end());
            ttest(map.find(1)->second == 10);
            ttest(map.find(3)->second == 30);
        }
    };

    class CaseBulkCrud : public TestCase {
    public:
        CaseBulkCrud() : TestCase("UnorderedMap 批量 CRUD 回归测试") {}

        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("批量插入 64 个元素，覆盖 rehash 路径");
            for (int i = 0; i < 64; ++i) {
                auto result = map.insert(std::make_pair(i, i * 10));
                ttest(result.second);
            }
            ttest(map.size() == 64);

            check("批量读取校验");
            for (int i = 0; i < 64; ++i) {
                auto it = map.find(i);
                ttest(it != map.end());
                ttest(it->second == i * 10);
            }

            action("批量更新部分元素");
            for (int i = 0; i < 64; i += 3) {
                auto result = map.insert_or_assign(i, i * 100);
                ttest(result.first != map.end());
            }
            for (int i = 0; i < 64; ++i) {
                auto it = map.find(i);
                ttest(it != map.end());
                if (i % 3 == 0) {
                    ttest(it->second == i * 100);
                } else {
                    ttest(it->second == i * 10);
                }
            }

            action("批量删除部分元素");
            int erased = 0;
            for (int i = 0; i < 64; i += 4) {
                erased += static_cast<int>(map.erase(i));
            }
            ttest(erased == 16);
            ttest(map.size() == 48);

            check("删除后剩余元素应保持可读");
            for (int i = 0; i < 64; ++i) {
                auto it = map.find(i);
                if (i % 4 == 0) {
                    ttest(it == map.end());
                } else {
                    ttest(it != map.end());
                }
            }

            action("clear 后应恢复为空容器");
            map.clear();
            ttest(map.empty());
            ttest(map.size() == 0);
            ttest(map.begin() == map.end());
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
        cases.push_back(new CaseCrudFlow());
        cases.push_back(new CaseOperatorIndex());
        cases.push_back(new CaseIteration());
        cases.push_back(new CaseCollisionHandling());
        cases.push_back(new CaseBulkCrud());
        cases.push_back(new CaseMoveConstruct());

        framework.add_category(new TestCategory("unordered_map",
                                                std::move(cases)));
    }
}  // namespace test::unordered_map
