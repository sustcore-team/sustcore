/**
 * @file string_view.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string_view 测试
 * @version alpha-1.1.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kio.h>
#include <test/string_view.h>

#include <cassert>
#include <cstdio>
#include <string_view>

using std::operator""sv;

namespace test::string_view {
    class CaseNullObjectCorner : public TestCase {
    public:
        CaseNullObjectCorner() : TestCase("边界条件-空对象") {}
        void _run() const noexcept override {
            std::string_view empty_sv;
            ttest(empty_sv.empty());
            ttest(empty_sv.size() == 0);
            ttest(empty_sv.data() == nullptr);
            ttest(empty_sv.find('a') == std::string_view::npos);
            ttest(empty_sv.rfind('a') == std::string_view::npos);
            ttest(empty_sv.find_first_of("abc") == std::string_view::npos);
        }
    };

    class CaseConstructor : public TestCase {
    public:
        CaseConstructor() : TestCase("构造器") {}
        void _run() const noexcept override {
            const char* s = "abc";
            // NOLINTNEXTLINE(bugprone-string-constructor)
            std::string_view sv(s, 0);
            ttest(sv.empty());
            ttest(sv.size() == 0);
            ttest(sv.data() == s);  // 虽然长度为0，但data指针应指向s

            std::string_view sv2("Hello", 5);
            ttest(sv2.size() == 5);
            ttest(sv2 == "Hello"sv);
        }
    };

    class CaseModifier : public TestCase {
    public:
        CaseModifier() : TestCase("修改器") {}
        void _run() const noexcept override {
            std::string_view sv = "Hello"sv;
            sv.remove_prefix(5);
            ttest(sv.empty());

            sv = "Hello"sv;
            sv.remove_suffix(5);
            ttest(sv.empty());

            sv = "Hello"sv;
            sv.remove_prefix(0);
            ttest(sv == "Hello"sv);

            std::string_view a = "aaa"sv, b = "bbb"sv;
            a.swap(b);
            ttest(a == "bbb"sv);
            ttest(b == "aaa"sv);
        }
    };

    class CaseSubstr : public TestCase {
    public:
        CaseSubstr() : TestCase("substr") {}
        void _run() const noexcept override {
            std::string_view sv = "Sustcore"sv;
            ttest(sv.substr(0, 0).empty());
            ttest(sv.substr(8, 0).empty());
            ttest(sv.substr(0, 100) == "Sustcore"sv);  // count > size
            ttest(sv.substr(4) == "core"sv);
            ttest(sv.substr(9).empty());
        }
    };

    class CaseFind : public TestCase {
    public:
        CaseFind() : TestCase("查找类方法") {}
        void _run() const noexcept override {
            std::string_view sv = "banana"sv;

            // Single char
            ttest(sv.find('a') == 1);
            ttest(sv.find('z') == std::string_view::npos);
            ttest(sv.rfind('a') == 5);
            ttest(sv.rfind('b', 0) == 0);

            // Substring
            ttest(sv.find("ana"sv) == 1);
            ttest(sv.find("ana"sv, 2) == 3);
            ttest(sv.rfind("ana"sv) == 3);
            ttest(sv.rfind("ana"sv, 2) == 1);

            // find_first_of / last_of
            ttest(sv.find_first_of("aeiou"sv) == 1);  // 'a'
            ttest(sv.find_last_of("aeiou"sv) == 5);   // 'a'

            // find_first_not_of / last_not_of
            ttest(sv.find_first_not_of("b"sv) == 1);  // 'a'
            ttest(sv.find_last_not_of("a"sv) == 4);   // 'n'

            // Empty substring find
            std::string_view abc = "abc"sv;
            ttest(abc.find(""sv) == 0);
            ttest(abc.rfind(""sv) == 3);
        }
    };

    class CaseCompare : public TestCase {
    public:
        CaseCompare() : TestCase("比较逻辑") {}
        void _run() const noexcept override {
            ttest("a"sv < "b"sv);
            ttest("a"sv < "ab"sv);
            ttest("abc"sv == "abc"sv);
            ttest("abc"sv != "abd"sv);
            ttest(std::string_view("abc", 2) == "ab"sv);

            // C++20 starts_with / ends_with / contains
            std::string_view sv = "Hello"sv;
            ttest(sv.starts_with(""sv));
            ttest(sv.ends_with(""sv));
            ttest(sv.contains(""sv));
            ttest(sv.starts_with("Hell"sv));
            ttest(sv.ends_with("lo"sv));
            ttest(sv.contains("ell"sv));
            ttest(!sv.starts_with("Hellos"sv));
        }
    };

    class CaseIterator : public TestCase {
    public:
        CaseIterator() : TestCase("迭代器") {}
        void _run() const noexcept override {
            std::string_view sv = "ABC"sv;
            auto it             = sv.begin();
            ttest(*it == 'A');
            ttest(*(++it) == 'B');
            ttest(*(++it) == 'C');
            ttest(++it == sv.end());

            size_t count = 0;
            for (char c [[maybe_unused]] : sv) count++;
            ttest(count == 3);
        }
    };

    void collect_tests(TestFramework& framework) {
        auto cases = util::ArrayList<TestCase*>();
        cases.push_back(new CaseNullObjectCorner());
        cases.push_back(new CaseConstructor());
        cases.push_back(new CaseModifier());
        cases.push_back(new CaseSubstr());
        cases.push_back(new CaseFind());
        cases.push_back(new CaseCompare());
        cases.push_back(new CaseIterator());

        framework.add_category(
            new TestCategory("string_view", std::move(cases)));
    }
}  // namespace test::string_view
