/**
 * @file optional_freestanding.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 验证 tay::optional 的 freestanding 编译契约。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#include <tay/optional.h>
#include <tay/variant.h>

#include <type_traits>
#include <utility>

namespace {
    struct value {
        int member;
    };

    constexpr bool optional_works() {
        tay::optional<value> item(tay::in_place, value{.member = 3});
        if (!item || item->member != 3)
            return false;
        item.reset();
        return item == tay::nullopt;
    }

    static_assert(optional_works());
    static_assert(std::is_nothrow_move_constructible_v<tay::optional<value>>);

    using variant_type = tay::variant<int, bool>;
    using visit_result = decltype(std::declval<const variant_type &>().visit(
        [](const auto &item) constexpr { return static_cast<int>(item); }));
    static_assert(std::is_same_v<visit_result, int>);
}  // namespace
