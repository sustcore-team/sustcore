/**
 * @file string_view.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::string_view 的切片、比较和查找操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/string_view.h>

#include <cassert>
#include <compare>
#include <type_traits>

using namespace tay::string_view_literals;

namespace {
    constexpr tay::string_view::size_type reference_find(tay::string_view text,
                                                         tay::string_view pattern,
                                                         tay::string_view::size_type position) {
        if (position > text.size()) {
            return tay::string_view::npos;
        }
        if (pattern.empty()) {
            return position;
        }
        if (pattern.size() > text.size() - position) {
            return tay::string_view::npos;
        }
        for (auto candidate = position; candidate <= text.size() - pattern.size(); ++candidate) {
            const auto selected = text.substr(candidate, pattern.size());
            if (selected && *selected == pattern) {
                return candidate;
            }
        }
        return tay::string_view::npos;
    }

    constexpr tay::string_view::size_type reference_rfind(tay::string_view text,
                                                          tay::string_view pattern,
                                                          tay::string_view::size_type position) {
        if (pattern.empty()) {
            return position < text.size() ? position : text.size();
        }
        if (pattern.size() > text.size()) {
            return tay::string_view::npos;
        }

        const auto last  = text.size() - pattern.size();
        const auto start = position < last ? position : last;
        for (auto candidate = start + 1; candidate > 0; --candidate) {
            const auto selected = text.substr(candidate - 1, pattern.size());
            if (selected && *selected == pattern) {
                return candidate - 1;
            }
        }
        return tay::string_view::npos;
    }

    void fill_binary(char *output, size_t length, size_t bits) {
        for (size_t index = 0; index < length; ++index) {
            output[index] = static_cast<char>('a' + ((bits >> index) & 1));
        }
    }

    void exhaustive_search_matches_reference() {
        char text_buffer[8]{};
        char pattern_buffer[7]{};

        for (size_t text_length = 0; text_length <= 8; ++text_length) {
            for (size_t text_bits = 0; text_bits < (size_t(1) << text_length); ++text_bits) {
                fill_binary(text_buffer, text_length, text_bits);
                const tay::string_view text(text_buffer, text_length);

                for (size_t pattern_length = 0; pattern_length <= 7; ++pattern_length) {
                    for (size_t pattern_bits = 0; pattern_bits < (size_t(1) << pattern_length);
                         ++pattern_bits)
                    {
                        fill_binary(pattern_buffer, pattern_length, pattern_bits);
                        const tay::string_view pattern(pattern_buffer, pattern_length);

                        for (size_t position = 0; position <= text_length + 1; ++position) {
                            assert(text.find(pattern, position) ==
                                   reference_find(text, pattern, position));
                            assert(text.rfind(pattern, position) ==
                                   reference_rfind(text, pattern, position));
                        }
                        assert(text.rfind(pattern) ==
                               reference_rfind(text, pattern, tay::string_view::npos));
                    }
                }
            }
        }
    }

    void pseudo_random_search_matches_reference() {
        char text_buffer[80]{};
        char pattern_buffer[40]{};
        size_t state = 0x9e3779b97f4a7c15ULL;

        for (size_t iteration = 0; iteration < 5000; ++iteration) {
            state                     ^= state << 7;
            state                     ^= state >> 9;
            const auto text_length     = state % sizeof(text_buffer);
            state                     ^= state << 8;
            const auto pattern_length  = state % sizeof(pattern_buffer);

            for (size_t index = 0; index < text_length; ++index) {
                state              ^= state << 7;
                state              ^= state >> 9;
                text_buffer[index]  = static_cast<char>(state >> 24);
            }
            for (size_t index = 0; index < pattern_length; ++index) {
                state                 ^= state << 7;
                state                 ^= state >> 9;
                pattern_buffer[index]  = static_cast<char>(state >> 24);
            }

            const tay::string_view text(text_buffer, text_length);
            const tay::string_view pattern(pattern_buffer, pattern_length);
            const auto position = state % (text_length + 2);
            assert(text.find(pattern, position) == reference_find(text, pattern, position));
            assert(text.rfind(pattern, position) == reference_rfind(text, pattern, position));
            assert(text.rfind(pattern) == reference_rfind(text, pattern, tay::string_view::npos));
        }
    }

    constexpr bool constexpr_interface_works() {
        tay::string_view view = "hello world";
        if (view.size() != 11 || view.length() != 11 || view.empty()) {
            return false;
        }
        if (view.front() != 'h' || view.back() != 'd' || view[4] != 'o') {
            return false;
        }

        auto character = view.at(1);
        if (!character || *character != 'e') {
            return false;
        }
        auto missing = view.at(view.size());
        if (missing || missing.error() != tay::error_code::OUT_OF_RANGE) {
            return false;
        }

        auto middle = view.substr(6, 5);
        if (!middle || *middle != "world") {
            return false;
        }
        auto invalid_substr = view.substr(12);
        if (invalid_substr || invalid_substr.error() != tay::error_code::OUT_OF_RANGE) {
            return false;
        }

        if (!view.starts_with("hello") || !view.starts_with('h') || !view.ends_with("world") ||
            !view.ends_with('d') || !view.contains("lo wo") || !view.contains(' '))
        {
            return false;
        }

        if (view.find("world") != 6 || view.find('o') != 4 || view.find("", 3) != 3 ||
            view.find("missing") != tay::string_view::npos || view.rfind('o') != 7 ||
            view.rfind("hello") != 0)
        {
            return false;
        }

        if (view.find_first_of("aeiou") != 1 || view.find_last_of("aeiou") != 7 ||
            view.find_first_not_of("hello") != 5 || view.find_last_not_of("world") != 5)
        {
            return false;
        }

        if (!(view > "hello") || !(view == "hello world") || view.compare("hello world") != 0) {
            return false;
        }

        auto prefix_compare = view.compare(0, 5, "hello");
        auto range_compare  = view.compare(6, 5, "world!", 5);
        auto bad_compare    = view.compare(20, 1, "x");
        if (!prefix_compare || *prefix_compare != 0 || !range_compare || *range_compare != 0 ||
            bad_compare || bad_compare.error() != tay::error_code::OUT_OF_RANGE)
        {
            return false;
        }

        tay::string_view trimmed = view;
        if (!trimmed.remove_prefix(6) || trimmed != "world") {
            return false;
        }
        if (!trimmed.remove_suffix(3) || trimmed != "wo") {
            return false;
        }
        const auto old_data = trimmed.data();
        const auto old_size = trimmed.size();
        auto invalid_remove = trimmed.remove_prefix(3);
        if (invalid_remove || trimmed.data() != old_data || trimmed.size() != old_size) {
            return false;
        }

        char copied[5]{};
        auto copy_count = view.copy(copied, 5, 6);
        if (!copy_count || *copy_count != 5 || tay::string_view(copied, 5) != "world") {
            return false;
        }
        auto invalid_copy = view.copy(copied, 1, 20);
        if (invalid_copy || invalid_copy.error() != tay::error_code::OUT_OF_RANGE) {
            return false;
        }

        tay::string_view left  = "left";
        tay::string_view right = "right";
        swap(left, right);
        return left == "right" && right == "left";
    }
}  // namespace

static_assert(std::is_same_v<tay::string_view::value_type, char>);
static_assert(std::is_same_v<decltype(tay::string_view{}.at(0)),
                             tay::expected<const char &, tay::error_code>>);
static_assert(noexcept(tay::string_view("text")));
static_assert(noexcept(tay::string_view{}.at(0)));
static_assert(noexcept(tay::string_view{}.substr()));
static_assert(noexcept(tay::string_view{}.remove_prefix(0)));
static_assert("literal"_sv.size() == 7);
static_assert("hello world"_sv.find("world") == 6);
static_assert("hello world"_sv.rfind('o') == 7);
static_assert("hello world"_sv.starts_with("hello"));
static_assert("hello world"_sv.ends_with("world"));
static_assert(tay::string_view_hash{}("hash") == tay::string_view_hash{}("hash"));

int main() {
    assert(constexpr_interface_works());

    const tay::string_view repeated = "xxababac--ababac--tail";
    assert(repeated.find("ababac") == 2);
    assert(repeated.find("ababac", 3) == 10);
    assert(repeated.rfind("ababac") == 10);
    assert(repeated.rfind("ababac", 9) == 2);
    assert(repeated.find("ababa") == 2);
    assert(repeated.rfind("ababa") == 10);

    const tay::string_view periodic = "aaaaaaaaaaaaaaaaab";
    assert(periodic.find("aaaaab") == 12);
    assert(periodic.rfind("aaaaaa") == 11);

    static constexpr char embedded[] = {'a', '\0', 'b'};
    constexpr tay::string_view view(embedded, 3);
    static_assert(view.size() == 3);
    static_assert(view.find('\0') == 1);

    constexpr tay::string_view empty;
    static_assert(empty.begin() == nullptr);
    static_assert(empty.end() == nullptr);
    static_assert(empty.find("") == 0);
    static_assert(empty.rfind("") == 0);

    static constexpr char embedded_text[]    = {'x', 'a',  '\0', 'b', 'c',  '\0', 'd',
                                                'a', '\0', 'b',  'c', '\0', 'd'};
    static constexpr char embedded_pattern[] = {'a', '\0', 'b', 'c', '\0', 'd'};
    const tay::string_view binary_text(embedded_text, sizeof(embedded_text));
    const tay::string_view binary_pattern(embedded_pattern, sizeof(embedded_pattern));
    assert(binary_text.find(binary_pattern) == 1);
    assert(binary_text.rfind(binary_pattern) == 7);

    exhaustive_search_matches_reference();
    pseudo_random_search_matches_reference();

    return 0;
}
