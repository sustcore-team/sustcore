/**
 * @file string.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::string 的构造、修改和比较操作。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/string.h>

#include <cassert>
#include <cstddef>
#include <new>
#include <type_traits>

namespace {
    struct allocation_state {
        bool fail            = false;
        size_t allocations   = 0;
        size_t deallocations = 0;
    };

    template <class T>
    struct checked_allocator {
        using value_type      = T;
        using is_always_equal = std::false_type;

        allocation_state* state = nullptr;

        constexpr explicit checked_allocator(allocation_state& shared) noexcept : state(&shared) {}

        template <class U>
        constexpr checked_allocator(const checked_allocator<U>& other) noexcept
            : state(other.state) {}

        tay::expected<T*, tay::error_code> try_allocate(size_t count) noexcept {
            if (state->fail) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            auto* memory = static_cast<T*>(::operator new(count * sizeof(T), std::nothrow));
            if (memory == nullptr) {
                return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                          tay::error_code::OUT_OF_MEMORY);
            }
            ++state->allocations;
            return memory;
        }

        void deallocate(T* memory, size_t) noexcept {
            ++state->deallocations;
            ::operator delete(memory);
        }

        template <class U>
        struct rebind {
            using other = checked_allocator<U>;
        };

        template <class>
        friend struct checked_allocator;
    };

    template <class T, class U>
    constexpr bool operator==(const checked_allocator<T>& left,
                              const checked_allocator<U>& right) noexcept {
        return left.state == right.state;
    }

    template <class T, class U>
    constexpr bool operator!=(const checked_allocator<T>& left,
                              const checked_allocator<U>& right) noexcept {
        return !(left == right);
    }

    template <class T>
    struct failing_allocator {
        using value_type      = T;
        using is_always_equal = std::true_type;

        tay::expected<T*, tay::error_code> try_allocate(size_t) noexcept {
            return tay::expected<T*, tay::error_code>(tay::unexpect,
                                                      tay::error_code::OUT_OF_MEMORY);
        }

        void deallocate(T*, size_t) noexcept {}
    };

}  // namespace

static_assert(sizeof(void*) != 8 || sizeof(tay::string<tay::allocator<char>>) == 32);
static_assert(std::is_default_constructible_v<tay::string<tay::allocator<char>>>);
static_assert(
    std::is_same_v<decltype(std::declval<tay::string<tay::allocator<char>>&>().append("x")),
                   tay::expected<tay::string<tay::allocator<char>>&, tay::error_code>>);
int main() {
    using host_string = tay::string<tay::allocator<char>>;
    tay::allocator<char> allocator;

    host_string empty;
    assert(empty.empty());

    host_string text("hello");
    assert(text == "hello");
    assert(text.starts_with("he"));
    assert(text.ends_with("lo"));
    assert(text.find("ll") == 2);

    host_string repeated(3, 'x');
    assert(repeated == "xxx");
    host_string counted("hello", 4);
    assert(counted == "hell");
    host_string viewed{tay::string_view("view")};
    assert(viewed == "view");
    host_string selected(tay::string_view("slice"), 1, 3);
    assert(selected == "lic");
    const char direct_range[] = "range";
    host_string from_range(direct_range, direct_range + 5);
    assert(from_range == "range");
    host_string listed({'o', 'k'});
    assert(listed == "ok");
    host_string explicit_allocator("explicit", allocator);
    assert(explicit_allocator == "explicit");

    auto empty_created = host_string::try_create();
    assert(empty_created && empty_created->empty());
    auto repeated_created = host_string::try_create(3, 'y');
    assert(repeated_created && *repeated_created == "yyy");
    auto counted_created = host_string::try_create("hello", 4);
    assert(counted_created && *counted_created == "hell");
    auto created = host_string::try_create("a dynamically allocated string");
    assert(created && *created == "a dynamically allocated string");
    auto viewed_created = host_string::try_create(tay::string_view("created view"));
    assert(viewed_created && *viewed_created == "created view");
    auto selected_created = host_string::try_create(tay::string_view("slice"), 1, 3);
    assert(selected_created && *selected_created == "lic");

    const char range_data[] = "a range that exceeds sso";
    auto range_created = host_string::try_create(range_data, range_data + sizeof(range_data) - 1);
    assert(range_created && *range_created == "a range that exceeds sso");
    auto listed_created = host_string::try_create({'t', 'a', 'y'});
    assert(listed_created && *listed_created == "tay");

    assert(text.append(" world"));
    assert(text == "hello world");
    assert(text.append(text.data() + 6, 5));
    assert(text == "hello worldworld");
    assert(text.capacity() >= text.size());

    assert(text.replace(6, 5, "kernel"));
    assert(text == "hello kernelworld");
    assert(text.erase(5, 1));
    assert(text == "hellokernelworld");
    assert(text.resize(5));
    assert(text == "hello");
    assert(text.resize(8, '!'));
    assert(text == "hello!!!");
    assert(text.pop_back());
    assert(text == "hello!!");
    assert(text.append('?'));
    assert(text == "hello!!?");

    host_string source("0123456789");
    host_string source_slice(source, 2, 4);
    assert(source_slice == "2345");
    auto source_created = host_string::try_create(source);
    assert(source_created && *source_created == source);
    auto source_slice_created = host_string::try_create(source, 2, 4);
    assert(source_slice_created && *source_slice_created == "2345");
    assert(text.assign(source, 2, 4));
    assert(text == "2345");
    assert(text.insert(2, source, 6, 2));
    assert(text == "236745");
    assert(text.append(source, 0, 2));
    assert(text == "23674501");
    auto compared = text.compare(0, 2, "23");
    assert(compared && *compared == 0);
    assert(text.find_first_of('7') == 3);
    assert(text.find_last_not_of('1') == 6);

    char copied[5]{};
    auto copy_count = text.copy(copied, 5);
    assert(copy_count && *copy_count == 5);
    assert(tay::string_view(copied, 5) == "23674");

    host_string local("left");
    host_string dynamic("this string is dynamically allocated");
    assert(local.swap(dynamic));
    assert(local == "this string is dynamically allocated");
    assert(dynamic == "left");

    const auto hash = tay::string_hash{}(text);
    assert(hash == tay::string_view_hash{}(tay::string_view(text)));

    allocation_state state;
    checked_allocator<char> checked(state);
    using checked_string = tay::string<checked_allocator<char>>;
    static_assert(!std::is_default_constructible_v<checked_string>);

    checked_string short_text("short", checked);
    assert(state.allocations == 0);
    assert(short_text.reserve(32));
    assert(state.allocations == 1);

    const auto old_view     = tay::string_view(short_text);
    const auto old_capacity = short_text.capacity();
    state.fail              = true;
    auto failed = short_text.append(" this append needs a substantially larger allocation");
    assert(!failed);
    assert(failed.error() == tay::error_code::OUT_OF_MEMORY);
    assert(tay::string_view(short_text) == old_view);
    assert(short_text.capacity() == old_capacity);

    auto failed_create =
        checked_string::try_create("this string cannot be dynamically allocated", checked);
    assert(!failed_create);
    assert(failed_create.error() == tay::error_code::OUT_OF_MEMORY);

    state.fail = false;
    assert(short_text.shrink_to_fit());
    assert(short_text == "short");

    allocation_state other_state;
    checked_allocator<char> other_allocator(other_state);
    checked_string other("other", other_allocator);
    auto unequal_swap = short_text.swap(other);
    assert(!unequal_swap);
    assert(unequal_swap.error() == tay::error_code::INVALID_ARGUMENT);
    assert(short_text == "short");
    assert(other == "other");

    using failing_string = tay::string<failing_allocator<char>>;
    auto default_failed =
        failing_string::try_create("this default allocator always fails dynamic allocation");
    assert(!default_failed);
    assert(default_failed.error() == tay::error_code::OUT_OF_MEMORY);
    return 0;
}
