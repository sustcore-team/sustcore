/**
 * @file stringview.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string_view 测试
 * @version alpha-1.0.1
 * @date 2026-03-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <string_view>
#include <cstdio>
#include <cassert>
#include <kio.h>

void test_string_view() {
    LOGGER::INFO("Starting string_view tests...");
    using namespace std::literals;

    // 1. 基础边界情况: 空对象
    {
        std::string_view empty_sv;
        assert(empty_sv.empty());
        assert(empty_sv.size() == 0);
        assert(empty_sv.data() == nullptr);
        assert(empty_sv.find('a') == std::string_view::npos);
        assert(empty_sv.rfind('a') == std::string_view::npos);
        assert(empty_sv.find_first_of("abc") == std::string_view::npos);
    }

    // 2. 构造函数边界
    {
        const char* s = "abc";
        std::string_view sv(s, 0);
        assert(sv.empty());
        assert(sv.size() == 0);
        assert(sv.data() == s); // 虽然长度为0，但data指针应指向s
    }

    // 3. 修改器边界: remove_prefix/suffix
    {
        std::string_view sv = "Hello"sv;
        sv.remove_prefix(5);
        assert(sv.empty());
        
        sv = "Hello"sv;
        sv.remove_suffix(5);
        assert(sv.empty());

        sv = "Hello"sv;
        sv.remove_prefix(0);
        assert(sv == "Hello"sv);
    }

    // 4. substr 边界
    {
        std::string_view sv = "Sustcore"sv;
        assert(sv.substr(0, 0).empty());
        assert(sv.substr(8, 0).empty());
        assert(sv.substr(0, 100) == "Sustcore"sv); // count > size
        assert(sv.substr(4) == "core"sv);
        // 注意: pos > size 在当前实现中返回空 sv
        assert(sv.substr(9).empty()); 
    }

    // 5. 查找方法边界: find, rfind, find_first_of...
    {
        std::string_view sv = "banana"sv;
        
        // Single char
        assert(sv.find('a') == 1);
        assert(sv.find('z') == std::string_view::npos);
        assert(sv.rfind('a') == 5);
        assert(sv.rfind('b', 0) == 0);

        // Substring
        assert(sv.find("ana"sv) == 1);
        assert(sv.find("ana"sv, 2) == 3);
        assert(sv.rfind("ana"sv) == 3);
        assert(sv.rfind("ana"sv, 2) == 1);

        // find_first_of / last_of
        assert(sv.find_first_of("aeiou"sv) == 1); // 'a'
        assert(sv.find_last_of("aeiou"sv) == 5);   // 'a'
        
        // find_first_not_of / last_not_of
        assert(sv.find_first_not_of("b"sv) == 1);  // 'a'
        assert(sv.find_last_not_of("a"sv) == 4);   // 'n'
    }

    // 6. 查找方法的高级边界 (空子串查找等)
    {
        std::string_view sv = "abc"sv;
        assert(sv.find(""sv) == 0);
        assert(sv.find(""sv, 1) == 1);
        assert(sv.find(""sv, 3) == 3);
        assert(sv.rfind(""sv) == 3);
        assert(sv.rfind(""sv, 1) == 1);
    }

    // 7. 比较逻辑边界
    {
        assert("a"sv < "b"sv);
        assert("a"sv < "ab"sv);
        assert("abc"sv == "abc"sv);
        assert("abc"sv != "abd"sv);
        assert(std::string_view("abc", 2) == "ab"sv);
    }

    // 8. 迭代器边界
    {
        std::string_view sv = "A"sv;
        auto it = sv.begin();
        assert(*it == 'A');
        assert(++it == sv.end());
    }

    // 9. C++20 starts_with / ends_with / contains 边界
    {
        std::string_view sv = "Hello"sv;
        assert(sv.starts_with(""sv));
        assert(sv.ends_with(""sv));
        assert(sv.contains(""sv));
        assert(!sv.starts_with("Hellos"sv));
    }

    LOGGER::INFO("All Enhanced string_view tests passed!");
}
