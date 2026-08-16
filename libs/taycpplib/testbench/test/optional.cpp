/**
 * @file optional.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::optional 的状态、生命周期和移动语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#include <tay/optional.h>

#include <cassert>
#include <type_traits>
#include <utility>

namespace {
    struct counted {
        static inline int live = 0;
        int value              = 0;

        explicit counted(int value) noexcept : value(value) {
            ++live;
        }
        counted(const counted &other) noexcept : value(other.value) {
            ++live;
        }
        counted(counted &&other) noexcept : value(other.value) {
            other.value = -1;
            ++live;
        }
        counted &operator=(const counted &) = default;
        counted &operator=(counted &&)      = default;
        ~counted() {
            --live;
        }
    };

    struct move_only {
        int value;

        explicit move_only(int value) noexcept : value(value) {}
        move_only(const move_only &)                = delete;
        move_only &operator=(const move_only &)     = delete;
        move_only(move_only &&) noexcept            = default;
        move_only &operator=(move_only &&) noexcept = default;
    };

    constexpr bool constexpr_states_work() {
        tay::optional<int> empty;
        if (empty || empty != tay::nullopt)
            return false;
        empty.emplace(7);
        if (!empty || *empty != 7 || empty.value_or(9) != 7)
            return false;
        empty = tay::nullopt;
        return !empty && empty.value_or(9) == 9;
    }

    static_assert(constexpr_states_work());
    static_assert(std::is_nothrow_move_constructible_v<tay::optional<int>>);
    static_assert(!std::is_copy_constructible_v<tay::optional<move_only>>);

    void test_states_and_assignment() {
        tay::optional<int> value = 42;
        assert(value.has_value() && value.value() == 42);
        assert(value.operator->() == &value.value());

        tay::optional<int> copy = value;
        assert(copy == value);
        copy = 9;
        assert(*copy == 9);
        copy = tay::nullopt;
        assert(copy == tay::nullopt);
        copy = value;
        assert(*copy == 42);
    }

    void test_lifetime_and_move_only() {
        assert(counted::live == 0);
        {
            tay::optional<counted> first(tay::in_place, 11);
            tay::optional<counted> second(first);
            assert(counted::live == 2 && second->value == 11);
            second.reset();
            assert(counted::live == 1);
            first.emplace(17);
            assert(counted::live == 1 && first->value == 17);
        }
        assert(counted::live == 0);

        tay::optional<move_only> source(tay::in_place, 23);
        tay::optional<move_only> target(std::move(source));
        assert(target && target->value == 23);
    }
}  // namespace

int main() {
    test_states_and_assignment();
    test_lifetime_and_move_only();
    return 0;
}
