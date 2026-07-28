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

    static_assert(std::is_constructible_v<ref_result, int&>);
    static_assert(!std::is_constructible_v<ref_result, int&&>);
    static_assert(!std::is_default_constructible_v<ref_result>);
    static_assert(std::is_same_v<
                  decltype(std::declval<const ref_result&>().value()), int&>);
    static_assert(std::is_same_v<decltype(std::declval<owned_result&>()
                                              .transform(as_reference)),
                                 ref_result>);
    static_assert(std::is_same_v<
                  decltype(std::declval<owned_result&>().transform(as_rvalue)),
                  owned_result>);
    static_assert(
        std::is_same_v<decltype(std::declval<tay::expected<box, error_code>&>()
                                    .transform(&box::get)),
                       ref_result>);
}  // namespace

extern "C" int tay_expected_freestanding_contract() {
    int value                                         = 4;
    ref_result result                                 = tay::Ok(value);
    tay::expected<const char*, error_code> text       = tay::Ok("text");
    int array[2]                                      = {1, 2};
    tay::expected<int(&)[2], error_code> array_result = tay::Ok(array);
    auto transformed = result.transform(as_reference)
                           .and_then([](int& current) {
                               return owned_result(tay::Ok(current + 1));
                           })
                           .transform_error([](error_code) { return 1; });
    return transformed ? transformed.value() + text.value()[0] - 't' +
                             array_result.value()[1] - 2
                       : transformed.error();
}
