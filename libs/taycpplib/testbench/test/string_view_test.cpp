#include <tay/string_view.h>

#include <cassert>
#include <compare>
#include <type_traits>

using namespace tay::string_view_literals;

namespace {
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
        if (invalid_substr ||
            invalid_substr.error() != tay::error_code::OUT_OF_RANGE)
        {
            return false;
        }

        if (!view.starts_with("hello") || !view.starts_with('h') ||
            !view.ends_with("world") || !view.ends_with('d') ||
            !view.contains("lo wo") || !view.contains(' '))
        {
            return false;
        }

        if (view.find("world") != 6 || view.find('o') != 4 ||
            view.find("", 3) != 3 ||
            view.find("missing") != tay::string_view::npos ||
            view.rfind('o') != 7 || view.rfind("hello") != 0)
        {
            return false;
        }

        if (view.find_first_of("aeiou") != 1 ||
            view.find_last_of("aeiou") != 7 ||
            view.find_first_not_of("hello") != 5 ||
            view.find_last_not_of("world") != 5)
        {
            return false;
        }

        if (!(view > "hello") || !(view == "hello world") ||
            view.compare("hello world") != 0)
        {
            return false;
        }

        auto prefix_compare = view.compare(0, 5, "hello");
        auto range_compare  = view.compare(6, 5, "world!", 5);
        auto bad_compare    = view.compare(20, 1, "x");
        if (!prefix_compare || *prefix_compare != 0 || !range_compare ||
            *range_compare != 0 || bad_compare ||
            bad_compare.error() != tay::error_code::OUT_OF_RANGE)
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
        if (invalid_remove || trimmed.data() != old_data ||
            trimmed.size() != old_size)
        {
            return false;
        }

        char copied[5]{};
        auto copy_count = view.copy(copied, 5, 6);
        if (!copy_count || *copy_count != 5 ||
            tay::string_view(copied, 5) != "world")
        {
            return false;
        }
        auto invalid_copy = view.copy(copied, 1, 20);
        if (invalid_copy ||
            invalid_copy.error() != tay::error_code::OUT_OF_RANGE)
        {
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
static_assert(tay::string_view_hash{}("hash") ==
              tay::string_view_hash{}("hash"));

int main() {
    assert(constexpr_interface_works());

    static constexpr char embedded[] = {'a', '\0', 'b'};
    constexpr tay::string_view view(embedded, 3);
    static_assert(view.size() == 3);
    static_assert(view.find('\0') == 1);

    constexpr tay::string_view empty;
    static_assert(empty.begin() == nullptr);
    static_assert(empty.end() == nullptr);
    static_assert(empty.find("") == 0);
    static_assert(empty.rfind("") == 0);

    return 0;
}
