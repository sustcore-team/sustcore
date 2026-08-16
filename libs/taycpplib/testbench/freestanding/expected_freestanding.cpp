/**
 * @file expected_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::expected 可在 freestanding 环境中编译和使用。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <tay/expected.h>

#include <type_traits>
#include <utility>

namespace {
    enum class error_code { failed };

    using owned_result = tay::expected<int, error_code>;
    using ref_result   = tay::expected<int&, error_code>;

    int& as_reference(int& value) {
        return value;
    }
    int&& as_rvalue(int& value) {
        return std::move(value);
    }

    struct box {
        int value;
        int& get() {
            return value;
        }
    };

    struct immovable {
        explicit immovable(int value) noexcept : value(value) {}
        immovable(const immovable&) = delete;
        immovable(immovable&&)      = delete;
        int value;
    };

    static_assert(std::is_constructible_v<ref_result, int&>);
    static_assert(
        std::is_constructible_v<tay::expected<immovable, error_code>, tay::in_place_t, int>);
    static_assert(!std::is_constructible_v<ref_result, int&&>);
    static_assert(!std::is_default_constructible_v<ref_result>);
    static_assert(std::is_same_v<decltype(std::declval<const ref_result&>().value()), int&>);
    static_assert(std::is_same_v<decltype(std::declval<owned_result&>().transform(as_reference)),
                                 ref_result>);
    static_assert(
        std::is_same_v<decltype(std::declval<owned_result&>().transform(as_rvalue)), owned_result>);
    static_assert(std::is_same_v<
                  decltype(std::declval<tay::expected<box, error_code>&>().transform(&box::get)),
                  ref_result>);

    owned_result macro_source(bool failure) {
        if (failure)
            return tay::Err(error_code::failed);
        return tay::Ok(4);
    }

    tay::expected<void, error_code> macro_void_source(bool failure) {
        if (failure)
            return tay::Err(error_code::failed);
        return tay::Ok();
    }

    owned_result macro_try(bool failure) {
        int value = TAY_TRY(macro_source(failure));
        return tay::Ok(value + 1);
    }

    owned_result macro_tryv(bool failure) {
        TAY_TRYV(macro_void_source(failure));
        return tay::Ok(5);
    }
}  // namespace

extern "C" int tay_expected_freestanding_contract() {
    int value                                         = 4;
    ref_result result                                 = tay::Ok(value);
    tay::expected<const char*, error_code> text       = tay::Ok("text");
    int array[2]                                      = {1, 2};
    tay::expected<int(&)[2], error_code> array_result = tay::Ok(array);
    auto transformed =
        result.transform(as_reference)
            .and_then([](int& current) { return owned_result(tay::Ok(current + 1)); })
            .transform_error([](error_code) { return 1; });
    auto macro_value = macro_try(false);
    auto macro_void  = macro_tryv(false);
    return transformed ? transformed.value() + text.value()[0] - 't' + array_result.value()[1] - 2 +
                             macro_value.value() - 5 + macro_void.value() - 5
                       : transformed.error();
}
