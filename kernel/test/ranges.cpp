/**
 * @file ranges.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ranges 测试
 * @version alpha-1.0.0
 * @date 2026-05-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <test/ranges.h>

#include <array>
#include <initializer_list>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace test::ranges {
    class CaseAccessArrays : public TestCase {
    public:
        CaseAccessArrays() : TestCase("数组访问") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            int values[] = {1, 2, 3};

            ttest(std::ranges::begin(values) == values);
            ttest(std::ranges::end(values) == values + 3);
            ttest(std::ranges::size(values) == 3);
            ttest(!std::ranges::empty(values));
            ttest(std::ranges::data(values) == values);
            ttest(*std::ranges::cbegin(values) == 1);

            ttest((std::is_same_v<std::ranges::iterator_t<decltype(values)>,
                                  int*>));
            ttest((std::is_same_v<std::ranges::range_value_t<decltype(values)>,
                                  int>));
            ttest((
                std::is_same_v<std::ranges::range_reference_t<decltype(values)>,
                               int&>));
        }
    };

    class CaseAccessContainers : public TestCase {
    public:
        CaseAccessContainers() : TestCase("容器访问") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::array<int, 3> arr{4, 5, 6};
            std::vector<int> vec;
            ttest(vec.push_back_nt(7).has_value());
            ttest(vec.push_back_nt(8).has_value());
            std::span<int> sp(arr.data(), arr.size());
            std::string_view sv("abc", 3);

            ttest(std::ranges::size(arr) == 3);
            ttest(*std::ranges::begin(arr) == 4);
            ttest(std::ranges::data(arr) == arr.data());

            ttest(std::ranges::size(vec) == 2);
            ttest(*std::ranges::begin(vec) == 7);
            ttest(std::ranges::data(vec) == vec.data());

            ttest(std::ranges::size(sp) == 3);
            ttest(std::ranges::data(sp) == arr.data());

            ttest(std::ranges::size(sv) == 3);
            ttest(*std::ranges::begin(sv) == 'a');
            ttest(std::ranges::data(sv) == sv.data());
        }
    };

    class CaseConcepts : public TestCase {
    public:
        CaseConcepts() : TestCase("概念与类型萃取") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::initializer_list<int> init{1, 2, 3};

            ttest((std::ranges::range<decltype(init)>));
            ttest((std::ranges::input_range<decltype(init)>));
            ttest((std::is_same_v<std::ranges::range_value_t<decltype(init)>,
                                  int>));
            ttest(
                (std::is_same_v<std::ranges::range_reference_t<decltype(init)>,
                                const int&>));
        }
    };

    class CaseViews : public TestCase {
    public:
        CaseViews() : TestCase("views") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            int values[] = {1, 2, 3, 4};

            auto all = std::views::all(values);
            ttest(*all.begin() == 1);
            values[0] = 9;
            ttest(*all.begin() == 9);
            ttest(all.size() == 4);

            auto alias = std::view::all(values);
            ttest(*alias.begin() == 9);

            auto counted = std::views::counted(values + 1, 2);
            ttest(!counted.empty());
            ttest(*counted.begin() == 2);
            ttest(counted.end() == values + 3);
        }
    };

    class CaseFind : public TestCase {
    public:
        CaseFind() : TestCase("find") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            int values[] = {1, 2, 3, 4};
            ttest(std::ranges::find(values, 3) == values + 2);
            ttest(std::ranges::find(values, 8) == values + 4);
            ttest(std::ranges::find(values + 1, values + 4, 2) == values + 1);

            std::vector<std::string> names;
            names.emplace_back("uart");
            names.emplace_back("virtio");
            names.emplace_back("plic");
            ttest(std::ranges::find(names, std::string("virtio")) ==
                  names.begin() + 1);
            ttest(std::ranges::find(names, std::string("missing")) ==
                  names.end());
        }
    };

    class CaseAlgorithms : public TestCase {
    public:
        CaseAlgorithms() : TestCase("algorithms") {}
        void _run(void* env [[maybe_unused]]) const noexcept override {
            std::vector<int> values = {4, 1, 3, 1, 2};

            std::ranges::sort(values);
            ttest(values[0] == 1);
            ttest(values[4] == 4);

            auto first_gt_two = std::ranges::find_if(
                values, [](int value) { return value > 2; });
            ttest(first_gt_two == values.begin() + 3);

            auto unique_end = std::ranges::unique(values);
            values.erase(unique_end, values.end());
            ttest(values.size() == 4);
            ttest(values[0] == 1 && values[1] == 2 && values[2] == 3 &&
                  values[3] == 4);

            std::vector<int> removed = {1, 2, 3, 2, 4};
            auto removed_end         = std::ranges::remove(removed, 2);
            removed.erase(removed_end, removed.end());
            ttest(removed.size() == 3);
            ttest(removed[0] == 1 && removed[1] == 3 && removed[2] == 4);

            std::vector<int> removed_if = {1, 2, 3, 4, 5};
            auto removed_if_end         = std::ranges::remove_if(
                removed_if, [](int value) { return value % 2 == 0; });
            removed_if.erase(removed_if_end, removed_if.end());
            ttest(removed_if.size() == 3);
            ttest(removed_if[0] == 1 && removed_if[1] == 3 &&
                  removed_if[2] == 5);

            ttest(std::ranges::min(3, 7) == 3);
            ttest(std::ranges::max(3, 7) == 7);
            ttest(std::ranges::abs(-5) == 5);
            ttest(std::ranges::clamp(9, 1, 5) == 5);
            ttest(std::ranges::clamp(-1, 1, 5) == 1);

            int arr[] = {1, 2, 3, 4};
            auto arr_mid = std::ranges::reverse(arr);
            ttest(arr[0] == 4 && arr[1] == 3 && arr[2] == 2 && arr[3] == 1);
            ttest(arr_mid == arr + 2);

            std::vector<int> reversed = {9, 8, 7};
            auto vec_mid =
                std::ranges::reverse(reversed.begin(), reversed.end());
            ttest(reversed[0] == 7 && reversed[1] == 8 && reversed[2] == 9);
            ttest(vec_mid == reversed.begin() + 1);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseAccessArrays());
        cases.push_back(new CaseAccessContainers());
        cases.push_back(new CaseConcepts());
        cases.push_back(new CaseViews());
        cases.push_back(new CaseFind());
        cases.push_back(new CaseAlgorithms());

        framework.add_category(new TestCategory("ranges", std::move(cases)));
    }
}  // namespace test::ranges
