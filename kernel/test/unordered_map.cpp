/**
 * @file unordered_map.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief unordered_map 测试
 * @version alpha-2.0.0
 * @date 2026-04-06
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/unordered_map.h>

#include <kio.h>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace test::unordered_map {

    struct ConstantHash {
        size_t operator()(int key [[maybe_unused]]) const noexcept {
            return 0;
        }
    };

    struct ModuloHash {
        size_t operator()(int key) const noexcept {
            return static_cast<size_t>(key % 10);
        }
    };

    struct ModuloEqual {
        bool operator()(int lhs, int rhs) const noexcept {
            return lhs % 10 == rhs % 10;
        }
    };

    static_assert(std::is_same<typename std::unordered_map<int, int>::value_type,
                               std::pair<const int, int>>::value,
                  "unordered_map::value_type 必须是 pair<const Key, T>");

    class CaseConstructionAndIteration : public TestCase {
    public:
        CaseConstructionAndIteration()
            : TestCase("UnorderedMap 构造、赋值与迭代边界测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> empty_map;

            expect("默认构造后应为空容器");
            ttest(empty_map.empty());
            ttest(empty_map.size() == 0);
            ttest(empty_map.begin() == empty_map.end());
            ttest(empty_map.cbegin() == empty_map.cend());

            action("填充基础数据");
            empty_map.emplace(1, 10);
            empty_map.emplace(2, 20);
            empty_map.emplace(3, 30);

            std::unordered_map<int, int> copied(empty_map);
            ttest(copied.size() == 3);
            ttest(copied.find(1) != copied.end());
            ttest(copied.find(2) != copied.end());
            ttest(copied.find(3) != copied.end());

            std::unordered_map<int, int> assigned;
            assigned = empty_map;
            ttest(assigned.size() == 3);
            ttest(assigned.find(1)->second == 10);
            ttest(assigned.find(2)->second == 20);
            ttest(assigned.find(3)->second == 30);

            std::unordered_map<int, int> moved(std::move(copied));
            ttest(moved.size() == 3);
            ttest(moved.find(1) != moved.end());
            ttest(copied.empty());

            std::unordered_map<int, int> move_assigned;
            move_assigned = std::move(assigned);
            ttest(move_assigned.size() == 3);
            ttest(move_assigned.find(1)->second == 10);
            ttest(assigned.empty());

            const std::unordered_map<int, int> &const_map = move_assigned;
            ttest(const_map.begin() != const_map.end());
            ttest(const_map.cbegin() != const_map.cend());
        }
    };

    class CaseLookupAndObservers : public TestCase {
    public:
        CaseLookupAndObservers()
            : TestCase("UnorderedMap 查找接口与观察器测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int, ModuloHash, ModuloEqual> map(
                8, ModuloHash(), ModuloEqual());

            action("插入基础数据");
            map.emplace(1, 10);
            map.emplace(2, 20);
            map.emplace(13, 130);

            check("find/count/contains 应正确工作");
            ttest(map.find(1) != map.end());
            ttest(map.find(11) != map.end());   // key_eq 按模 10 相等
            ttest(map.find(4) == map.end());
            ttest(map.count(2) == 1);
            ttest(map.count(5) == 0);
            ttest(map.contains(11));
            ttest(!map.contains(4));

            check("equal_range 在唯一键语义下应返回长度 0 或 1");
            auto found_range = map.equal_range(2);
            ttest(found_range.first != map.end());
            ttest(found_range.first->second == 20);
            if (found_range.second != map.end()) {
                ttest(found_range.second->first != found_range.first->first);
            }

            auto miss_range = map.equal_range(4);
            ttest(miss_range.first == map.end());
            ttest(miss_range.second == map.end());

            check("观察器应返回构造时传入的可用对象");
            ttest(map.hash_function()(3) == 3);
            ttest(map.hash_function()(1) == map.hash_function()(11));
            ttest(map.key_eq()(21, 11));
            ttest(!map.key_eq()(21, 12));
        }
    };

    class CaseElementAccess : public TestCase {
    public:
        CaseElementAccess()
            : TestCase("UnorderedMap 元素访问与更新入口测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map;

            action("operator[] 未命中时应默认插入");
            int &slot = map[7];
            ttest(map.size() == 1);
            ttest(slot == 0);
            ttest(map.find(7) != map.end());

            action("operator[] 命中时应返回可写引用");
            map[7] = 77;
            ttest(map.size() == 1);
            ttest(map.at(7) == 77);

            action("at() 命中时应返回已存值");
            map.emplace(9, 99);
            ttest(map.at(9) == 99);

            comment("at() 未命中异常路径已按 cppreference 语义保留；当前测试只覆盖命中路径。");

            const std::unordered_map<int, int> &const_map = map;
            ttest(const_map.at(7) == 77);
            ttest(const_map.find(9) != const_map.end());
        }
    };

    class CaseInsertAndAssign : public TestCase {
    public:
        CaseInsertAndAssign()
            : TestCase("UnorderedMap 插入、hint、emplace、assign 测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            using map_type = std::unordered_map<int, int>;
            map_type map;

            action("insert(value_type) 插入新键");
            auto first = map.insert(typename map_type::value_type(1, 10));
            ttest(first.second);
            ttest(first.first->second == 10);

            action("insert(value_type) 对重复键返回 false");
            auto duplicate = map.insert(typename map_type::value_type(1, 99));
            ttest(!duplicate.second);
            ttest(map.size() == 1);
            ttest(duplicate.first->second == 10);

            action("insert(hint, value) 至少应完成插入");
            auto hinted = map.insert(map.cbegin(),
                                     typename map_type::value_type(2, 20));
            ttest(hinted != map.end());
            ttest(map.find(2) != map.end());

            action("emplace 对新键插入、对旧键返回 false");
            auto emplaced = map.emplace(3, 30);
            ttest(emplaced.second);
            auto emplace_dup = map.emplace(3, 300);
            ttest(!emplace_dup.second);
            ttest(map.find(3)->second == 30);

            action("try_emplace 不应覆盖已有值");
            auto try_new = map.try_emplace(4, 40);
            ttest(try_new.second);
            auto try_dup = map.try_emplace(4, 400);
            ttest(!try_dup.second);
            ttest(map.find(4)->second == 40);

            action("insert_or_assign 对旧键覆盖，对新键插入");
            auto updated = map.insert_or_assign(4, 444);
            ttest(!updated.second);
            ttest(map.find(4)->second == 444);
            auto inserted = map.insert_or_assign(5, 50);
            ttest(inserted.second);
            ttest(map.find(5)->second == 50);
        }
    };

    class CaseEraseAndClear : public TestCase {
    public:
        CaseEraseAndClear()
            : TestCase("UnorderedMap 删除接口与 clear 复用测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int, ConstantHash> map(4);

            action("构造冲突链");
            map.emplace(1, 10);
            map.emplace(2, 20);
            map.emplace(3, 30);
            map.emplace(4, 40);

            action("erase(key) 删除存在键");
            ttest(map.erase(2) == 1);
            ttest(map.find(2) == map.end());
            ttest(map.size() == 3);

            action("erase(key) 删除不存在键");
            ttest(map.erase(99) == 0);
            ttest(map.size() == 3);

            action("erase(iterator) 删除链表头/中间节点后其余元素仍可达");
            auto it = map.find(4);
            ttest(it != map.end());
            auto next = map.erase(it);
            ttest(map.find(4) == map.end());
            ttest(next != map.end());
            ttest(map.find(1) != map.end());
            ttest(map.find(3) != map.end());

            action("erase(range) 删除一段元素");
            auto first = map.begin();
            auto last  = map.begin();
            ++last;
            map.erase(first, last);
            ttest(map.size() == 1);

            action("clear 后容器应可复用");
            map.clear();
            ttest(map.empty());
            ttest(map.begin() == map.end());
            map.emplace(8, 80);
            ttest(map.size() == 1);
            ttest(map.find(8) != map.end());
        }
    };

    class CaseRehashReserveAndLoadFactor : public TestCase {
    public:
        CaseRehashReserveAndLoadFactor()
            : TestCase("UnorderedMap rehash、reserve 与负载因子测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> map(1);

            action("批量插入触发多次扩容");
            for (int i = 0; i < 64; ++i) {
                auto result = map.emplace(i, i * 10);
                ttest(result.second);
            }
            ttest(map.size() == 64);

            check("rehash 后所有元素都应可查");
            for (int i = 0; i < 64; ++i) {
                auto it = map.find(i);
                ttest(it != map.end());
                ttest(it->second == i * 10);
            }

            check("load_factor / max_load_factor 接口可用");
            float old_max_load = map.max_load_factor();
            ttest(old_max_load > 0.0f);
            ttest(map.load_factor() >= 0.0f);

            action("调整最大负载因子并继续插入");
            map.max_load_factor(0.5f);
            ttest(map.max_load_factor() == 0.5f);
            map.reserve(256);
            for (int i = 64; i < 96; ++i) {
                auto result = map.insert_or_assign(i, i * 10);
                ttest(result.first != map.end());
            }
            ttest(map.size() == 96);

            check("reserve / rehash 后数据完整");
            for (int i = 0; i < 96; ++i) {
                auto it = map.find(i);
                ttest(it != map.end());
                ttest(it->second == i * 10);
            }
        }
    };

    class CaseSwapAndAllocator : public TestCase {
    public:
        CaseSwapAndAllocator()
            : TestCase("UnorderedMap swap 与 allocator 接口测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            std::unordered_map<int, int> lhs;
            std::unordered_map<int, int> rhs;

            lhs.emplace(1, 10);
            lhs.emplace(2, 20);
            rhs.emplace(9, 90);

            action("swap 前保存 allocator 接口可见性");
            auto lhs_alloc = lhs.get_allocator();
            auto rhs_alloc = rhs.get_allocator();
            (void)lhs_alloc;
            (void)rhs_alloc;

            action("swap 之后两边数据应交换");
            lhs.swap(rhs);
            ttest(lhs.size() == 1);
            ttest(rhs.size() == 2);
            ttest(lhs.find(9) != lhs.end());
            ttest(rhs.find(1) != rhs.end());
            ttest(rhs.find(2) != rhs.end());
        }
    };

    class CaseStress : public TestCase {
    public:
        CaseStress() : TestCase("UnorderedMap 大规模压力回归测试") {}

        void _run(void *env [[maybe_unused]]) const noexcept override {
            constexpr int total_count  = 2048;
            constexpr int update_step  = 3;
            constexpr int erase_step   = 5;

            std::unordered_map<int, int> map(1);
            int inserted = 0;
            int updated  = 0;
            int erased   = 0;
            long long remaining_sum = 0;
            int remaining_count = 0;

            action("批量插入 2048 个元素，覆盖连续扩容与重排路径");
            for (int i = 0; i < total_count; ++i) {
                auto result = map.emplace(i, i + 1);
                ttest(result.second);
                inserted += 1;
            }
            ttest(map.size() == static_cast<size_t>(total_count));

            action("按步长 3 批量更新，验证 insert_or_assign 压力路径");
            for (int i = 0; i < total_count; i += update_step) {
                auto result = map.insert_or_assign(i, i * 10);
                ttest(result.first != map.end());
                updated += 1;
            }

            check("按步长 5 批量删除，验证 erase(key) 在高负载下的正确性");
            for (int i = 0; i < total_count; i += erase_step) {
                erased += static_cast<int>(map.erase(i));
            }

            for (int i = 0; i < total_count; ++i) {
                auto it = map.find(i);
                if (i % erase_step == 0) {
                    ttest(it == map.end());
                } else {
                    ttest(it != map.end());
                    if (i % update_step == 0) {
                        ttest(it->second == i * 10);
                    } else {
                        ttest(it->second == i + 1);
                    }
                    remaining_count += 1;
                    remaining_sum += it->second;
                }
            }

            ttest(map.size() == static_cast<size_t>(total_count - erased));

            action("clear 后再次复用，验证容器在压力后仍保持可用");
            map.clear();
            ttest(map.empty());
            for (int i = 0; i < 128; ++i) {
                auto result = map.emplace(i, i * 2);
                ttest(result.second);
            }
            ttest(map.size() == 128);
            ttest(map.find(127) != map.end());
            ttest(map.find(127)->second == 254);

            kprintfln(
                "    - [压力测试结果] inserted=%d updated=%d erased=%d remaining=%d remaining_sum=%d reused=%d",
                inserted, updated, erased, remaining_count,
                static_cast<int>(remaining_sum), 128);
        }
    };

    void collect_tests(TestFramework &framework) {
        auto cases = util::ArrayList<TestCase *>();
        cases.push_back(new CaseConstructionAndIteration());
        cases.push_back(new CaseLookupAndObservers());
        cases.push_back(new CaseElementAccess());
        cases.push_back(new CaseInsertAndAssign());
        cases.push_back(new CaseEraseAndClear());
        cases.push_back(new CaseRehashReserveAndLoadFactor());
        cases.push_back(new CaseSwapAndAllocator());
        // cases.push_back(new CaseStress());

        framework.add_category(
            new TestCategory("unordered_map", std::move(cases)));
    }
}  // namespace test::unordered_map
