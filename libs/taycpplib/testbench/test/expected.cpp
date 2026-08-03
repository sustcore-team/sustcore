/**
 * @file expected.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::expected 的值、错误和引用语义。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/expected.h>
#include <tay/utility.h>

#include <cassert>
#include <type_traits>
#include <utility>

namespace {
    enum class error_code { failure, missing };

    struct move_only {
        int value;

        explicit move_only(int value) : value(value) {}
        move_only(const move_only&)            = delete;
        move_only& operator=(const move_only&) = delete;
        move_only(move_only&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        move_only& operator=(move_only&& other) noexcept {
            value       = other.value;
            other.value = -1;
            return *this;
        }
    };

    struct immovable {
        static inline int live = 0;
        int value;

        explicit immovable(int value) noexcept : value(value) {
            ++live;
        }
        immovable(const immovable&)            = delete;
        immovable& operator=(const immovable&) = delete;
        immovable(immovable&&)                 = delete;
        immovable& operator=(immovable&&)      = delete;
        ~immovable() {
            --live;
        }
    };

    struct counted {
        static inline int live = 0;
        int value;

        explicit counted(int value = 0) noexcept : value(value) {
            ++live;
        }
        counted(const counted& other) : value(other.value) {
            ++live;
        }
        counted(counted&& other) noexcept : value(other.value) {
            other.value = -1;
            ++live;
        }
        counted& operator=(const counted&) = default;
        counted& operator=(counted&&)      = default;
        ~counted() {
            --live;
        }
    };

    struct throwing_value {
        static inline bool throw_copy = false;
        int value;

        explicit throwing_value(int value = 0) : value(value) {}
        throwing_value(const throwing_value& other) : value(other.value) {
            if (throw_copy) {
                throw 17;
            }
        }
        throwing_value(throwing_value&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        throwing_value& operator=(const throwing_value&) = default;
        throwing_value& operator=(throwing_value&&)      = default;
    };

    struct throwing_swap_value {
        static inline bool throw_move = false;
        int value;

        explicit throwing_swap_value(int value = 0) : value(value) {}
        throwing_swap_value(const throwing_swap_value&) = default;
        throwing_swap_value(throwing_swap_value&& other) : value(other.value) {
            if (throw_move) {
                throw 23;
            }
            other.value = -1;
        }
        throwing_swap_value& operator=(const throwing_swap_value&) = default;
        throwing_swap_value& operator=(throwing_swap_value&& other) {
            value       = other.value;
            other.value = -1;
            return *this;
        }
    };

    struct throwing_error {
        static inline bool throw_copy = false;
        int value;

        explicit throwing_error(int value = 0) : value(value) {}
        throwing_error(const throwing_error& other) : value(other.value) {
            if (throw_copy) {
                throw 29;
            }
        }
        throwing_error(throwing_error&& other) noexcept : value(other.value) {
            other.value = -1;
        }
        throwing_error& operator=(const throwing_error&) = default;
        throwing_error& operator=(throwing_error&&)      = default;
    };

    int& identity_ref(int& value) {
        return value;
    }
    int&& identity_rref(int& value) {
        return std::move(value);
    }
    int increment(int value) {
        return value + 1;
    }

    struct box {
        int value;
        int& get() {
            return value;
        }
    };

    using ref_result   = tay::expected<int&, error_code>;
    using owned_result = tay::expected<int, error_code>;

    static_assert(std::is_constructible_v<ref_result, int&>);
    static_assert(!std::is_constructible_v<ref_result, int&&>);
    static_assert(!std::is_default_constructible_v<ref_result>);
    static_assert(std::is_same_v<decltype(std::declval<const ref_result&>().value()), int&>);
    static_assert(
        std::is_same_v<decltype(std::declval<tay::expected<const int&, error_code>&>().value()),
                       const int&>);
    static_assert(!std::is_convertible_v<decltype(tay::Ok(42)), ref_result>);
    static_assert(std::is_convertible_v<decltype(tay::Ok()), tay::expected<void, error_code>>);
    static_assert(!std::is_convertible_v<decltype(tay::Ok()), owned_result>);
    static_assert(std::is_same_v<decltype(std::declval<owned_result&>().transform(identity_ref)),
                                 tay::expected<int&, error_code>>);
    static_assert(std::is_same_v<decltype(std::declval<owned_result&>().transform(identity_rref)),
                                 tay::expected<int, error_code>>);

    void test_basic_states() {
        tay::expected<int, error_code> value = tay::Ok(42);
        assert(value && value.value() == 42 && *value == 42);
        assert(value.operator->() == std::addressof(value.value()));

        tay::expected<int, error_code> error = tay::Err(error_code::failure);
        assert(!error && error.error() == error_code::failure);

        error = value;
        assert(error && error.value() == 42);
        error = tay::Err(error_code::missing);
        assert(!error && error.error() == error_code::missing);
        error.emplace(9);
        assert(error && error.value() == 9);

        tay::expected<void, error_code> done = tay::Ok();
        assert(done);
        done = tay::Err(error_code::failure);
        assert(!done && done.error() == error_code::failure);
        done.emplace();
        assert(done);

        tay::expected<int, int> same_value = tay::Ok(3);
        tay::expected<int, int> same_error = tay::Err(4);
        assert(same_value.value() == 3);
        assert(same_error.error() == 4);

        tay::expected<const char*, error_code> text = tay::Ok("text");
        assert(text.value()[0] == 't');

        int array[2]                                   = {5, 6};
        tay::expected<int(&)[2], error_code> array_ref = tay::Ok(array);
        array_ref.value()[1]                           = 7;
        assert(array[1] == 7);

        tay::expected<int (&)(int), error_code> function_ref = tay::Ok(increment);
        assert(function_ref.value()(4) == 5);
    }

    void test_lifetimes_and_move_only() {
        assert(counted::live == 0);
        {
            tay::expected<counted, counted> result = tay::Ok(counted{1});
            assert(counted::live == 1);
            result = tay::Err(counted{2});
            assert(counted::live == 1);
            result.emplace(3);
            assert(counted::live == 1 && result->value == 3);
        }
        assert(counted::live == 0);

        tay::expected<move_only, move_only> result = tay::Ok(move_only{5});
        assert(result->value == 5);
        result = tay::Err(move_only{8});
        assert(!result && result.error().value == 8);
        result = move_only{11};
        assert(result && result->value == 11);
    }

    void test_in_place_immovable() {
        {
            tay::expected<immovable, error_code> value(tay::in_place, 42);
            assert(value && value->value == 42);
            assert(immovable::live == 1);
        }
        assert(immovable::live == 0);

        {
            tay::expected<immovable, error_code> initialized(
                tay::try_in_place,
                [](immovable& value) noexcept -> tay::expected<void, error_code> {
                    value.value += 1;
                    return {};
                },
                8);
            assert(initialized && initialized->value == 9);
        }
        assert(immovable::live == 0);

        {
            tay::expected<immovable, error_code> failed(
                tay::try_in_place,
                [](immovable&) noexcept -> tay::expected<void, error_code> {
                    return tay::Err(error_code::failure);
                },
                7);
            assert(!failed && failed.error() == error_code::failure);
            assert(immovable::live == 0);
        }
    }

    void test_reference_rebinding() {
        int first        = 1;
        int second       = 2;
        ref_result left  = tay::Ok(first);
        ref_result right = tay::Ok(second);
        left             = right;
        assert(std::addressof(left.value()) == std::addressof(second));
        assert(first == 1 && second == 2);

        left.value() = 7;
        assert(second == 7);
        assert(left.operator->() == std::addressof(second));
        assert(std::addressof(*left) == std::addressof(second));

        left = tay::Err(error_code::failure);
        assert(!left);
        left.emplace(first);
        assert(std::addressof(left.value()) == std::addressof(first));

        left.swap(right);
        assert(std::addressof(left.value()) == std::addressof(second));
        assert(std::addressof(right.value()) == std::addressof(first));
    }

    void test_monadic_operations() {
        int value            = 10;
        ref_result reference = tay::Ok(value);

        auto referenced    = reference.transform(identity_ref);
        referenced.value() = 12;
        assert(value == 12);

        auto owned = reference.transform(identity_rref);
        static_assert(std::is_same_v<decltype(owned), owned_result>);
        assert(owned.value() == 12);

        tay::expected<box, error_code> boxed = tay::Ok(box{14});
        auto member_reference                = boxed.transform(&box::get);
        static_assert(std::is_same_v<decltype(member_reference), ref_result>);
        member_reference.value() = 15;
        assert(boxed->value == 15);

        auto chained = reference.and_then(
            [](int& item) { return tay::expected<long, error_code>(tay::Ok(long(item + 1))); });
        assert(chained && chained.value() == 13);

        ref_result failed = tay::Err(error_code::failure);
        auto recovered =
            failed.or_else([&](error_code) { return tay::expected<int&, long>(tay::Ok(value)); });
        static_assert(std::is_same_v<decltype(recovered), tay::expected<int&, long>>);
        assert(std::addressof(recovered.value()) == std::addressof(value));

        auto changed_error = failed.transform_error([](error_code) { return 99L; });
        static_assert(std::is_same_v<decltype(changed_error), tay::expected<int&, long>>);
        assert(!changed_error && changed_error.error() == 99L);

        auto kept_reference = reference.transform_error([](error_code) { return 0L; });
        assert(std::addressof(kept_reference.value()) == std::addressof(value));

        tay::expected<void, error_code> done = tay::Ok();
        auto void_chain                      = done.transform([] { return 21; });
        assert(void_chain && void_chain.value() == 21);

        const owned_result constant = tay::Ok(4);
        auto const_rvalue =
            std::move(constant).transform([](const int&& item) { return item + 2; });
        assert(const_rvalue.value() == 6);
    }

    void test_match_and_visit() {
        tay::expected<int, error_code> value = tay::Ok(41);
        auto value_result                    = value.match(tay::overloaded{
            [](int item) { return item + 1; },
            [](error_code) { return -1; },
        });
        assert(value_result == 42);

        tay::expected<int, error_code> failed = tay::Err(error_code::missing);
        auto error_result                     = failed.visit(tay::overloaded{
            [](int) { return 1; },
            [](error_code error) { return error == error_code::missing ? 2 : 3; },
        });
        assert(error_result == 2);

        tay::expected<void, error_code> done = tay::Ok();
        auto void_value                      = done.match(tay::overloaded{
            [] { return 7; },
            [](error_code) { return -1; },
        });
        assert(void_value == 7);

        done            = tay::Err(error_code::failure);
        auto void_error = done.visit(tay::overloaded{
            [] { return 7; },
            [](error_code) { return -1; },
        });
        assert(void_error == -1);

        int referenced_value                       = 9;
        tay::expected<int&, error_code> referenced = tay::Ok(referenced_value);
        auto reference_result                      = referenced.match(tay::overloaded{
            [](int& item) { return item; },
            [](error_code) { return -1; },
        });
        assert(reference_result == 9);
    }

    void test_exception_state_restoration() {
        tay::expected<throwing_value, error_code> target = tay::Err(error_code::missing);
        tay::expected<throwing_value, error_code> source = tay::Ok(throwing_value{31});

        throwing_value::throw_copy = true;
        try {
            target = source;
            assert(false);
        } catch (int code) {
            assert(code == 17);
        }
        throwing_value::throw_copy = false;
        assert(!target && target.error() == error_code::missing);
        assert(source && source->value == 31);

        tay::expected<int, throwing_error> value_target = tay::Ok(37);
        tay::expected<int, throwing_error> error_source = tay::Err(throwing_error{38});
        throwing_error::throw_copy                      = true;
        try {
            value_target = error_source;
            assert(false);
        } catch (int code) {
            assert(code == 29);
        }
        throwing_error::throw_copy = false;
        assert(value_target && value_target.value() == 37);
        assert(!error_source && error_source.error().value == 38);

        tay::expected<throwing_swap_value, error_code> value = tay::Ok(throwing_swap_value{41});
        tay::expected<throwing_swap_value, error_code> error = tay::Err(error_code::failure);
        throwing_swap_value::throw_move                      = true;
        try {
            value.swap(error);
            assert(false);
        } catch (int code) {
            assert(code == 23);
        }
        throwing_swap_value::throw_move = false;
        assert(value.has_value());
        assert(!error && error.error() == error_code::failure);
    }
}  // namespace

int main() {
    test_basic_states();
    test_lifetimes_and_move_only();
    test_in_place_immovable();
    test_reference_rebinding();
    test_monadic_operations();
    test_match_and_visit();
    test_exception_state_restoration();
    return 0;
}
