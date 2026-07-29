/**
 * @file format.h
 * @author theflysong (song_of_the_fly@163.com)
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Allocation-free typed formatting to buffered callbacks.
 * @version 0.1.0-dev.1
 * @date 2026-07-30
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/expected.h>
#include <tay/fmt/context.h>
#include <tay/fmt/core.h>
#include <tay/fmt/engine.h>
#include <tay/fmt/formatters.h>

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace tay {
    template <class... Args>
    template <std::size_t N>
    consteval format_string<Args...>::format_string(
        const char (&text)[N]) noexcept
        : text_(text, N - 1) {
        detail::validate_format<Args...>(text_);
    }

    template <std::size_t ChunkSize = 256, class WriteCallback, class... Args>
    [[nodiscard]] expected<std::size_t, format_error> format_to(
        WriteCallback&& write, format_string<std::type_identity_t<Args>...> fmt,
        Args&&... args) {
        static_assert(ChunkSize > 0,
                      "tay::format_to requires a non-zero chunk size");

        using callback_type = std::remove_reference_t<WriteCallback>;
        using result_type   = std::remove_cvref_t<
              std::invoke_result_t<callback_type&, const char*, std::size_t>>;

        static_assert(
            std::is_integral_v<result_type> && std::is_signed_v<result_type>,
            "tay::format_to callback must return a signed integer");
        static_assert(
            ChunkSize <= static_cast<std::size_t>(
                             std::numeric_limits<result_type>::max()),
            "tay::format_to chunk size must fit in the callback result type");

        detail::callback_sink<ChunkSize, callback_type> sink(write);
        detail::basic_format_context context(sink);
        detail::execute_format(context, fmt.get(), std::forward<Args>(args)...);
        sink.finish();
        return sink.result();
    }
}  // namespace tay
