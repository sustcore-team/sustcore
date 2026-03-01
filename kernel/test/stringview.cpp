/**
 * @file stringview.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief string_view测试
 * @version alpha-1.0.0
 * @date 2026-03-01
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <string_view>
#include <cstdio>
#include <cassert>
#include <kio.h>

void test_string_view() {
    LOGGER::INFO("Running string_view tests...");

    // 1. 构造函数测试
    std::string_view sv1;
    assert(sv1.empty());
    assert(sv1.size() == 0);
    assert(sv1.data() == nullptr);

    const char* s = "Hello, Sustcore!";
    std::string_view sv2(s);
    assert(!sv2.empty());
    assert(sv2.size() == 16);
    assert(sv2.data() == s);

    std::string_view sv3(s, 5);
    assert(sv3.size() == 5);
    assert(sv3.at(0) == 'H');
    assert(sv3.at(4) == 'o');

    std::string_view sv4 = sv2;
    assert(sv4.size() == 16);
    assert(sv4.data() == s);

    // 2. 元素访问测试
    assert(sv2[0] == 'H');
    assert(sv2.at(7) == 'S');
    assert(sv2.front() == 'H');
    assert(sv2.back() == '!');

    // 3. 成员函数测试
    // substr
    std::string_view sub = sv2.substr(7, 8);
    assert(sub.size() == 8);
    assert(sub.at(0) == 'S');
    assert(std::string_view("Sustcore").compare(sub) == 0);

    // compare
    assert(sv2.compare(sv2) == 0);
    assert(std::string_view("abc").compare("abd") < 0);
    assert(std::string_view("abd").compare("abc") > 0);
    assert(std::string_view("abc").compare("abcd") < 0);

    // find
    assert(sv2.find("Sustcore") == 7);
    assert(sv2.find("Sust") == 7);
    assert(sv2.find("core") == 11);
    assert(sv2.find("none") == std::string_view::npos);
    assert(sv2.find("H", 1) == std::string_view::npos);

    LOGGER::INFO("string_view tests passed!");
}
