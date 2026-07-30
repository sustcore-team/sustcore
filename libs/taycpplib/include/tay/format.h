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

#include <tay/allocator.h>
#include <tay/err.h>
#include <tay/expected.h>
#include <tay/fmt/context.h>
#include <tay/fmt/core.h>
#include <tay/fmt/engine.h>
#include <tay/fmt/formatters.h>
#include <tay/string.h>

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace tay {
    namespace detail {
        template <class OutputIt>
        class limited_iterator_sink {
        private:
            OutputIt output_;
            std::size_t remaining_ = 0;

        public:
            constexpr limited_iterator_sink(OutputIt output, std::size_t size)
                : output_(std::move(output)), remaining_(size) {}

            constexpr void put(char character) {
                if (stopped()) {
                    return;
                }
                *output_++ = character;
                --remaining_;
            }

            constexpr void write(const char* data, std::size_t size) {
                for (std::size_t index = 0; index < size && !stopped(); ++index)
                {
                    put(data[index]);
                }
            }

            [[nodiscard]] constexpr bool stopped() const noexcept {
                return remaining_ == 0;
            }

            [[nodiscard]] constexpr OutputIt take_output() {
                return std::move(output_);
            }
        };

        template <class Allocator>
        class string_sink {
        private:
            string<Allocator>* output_ = nullptr;
            error_code error_          = error_code::NONE;

        public:
            constexpr explicit string_sink(string<Allocator>& output) noexcept
                : output_(&output) {}

            constexpr void put(char character) {
                if (stopped()) {
                    return;
                }
                auto appended = output_->push_back(character);
                if (!appended) {
                    error_ = appended.error();
                }
            }

            constexpr void write(const char* data, std::size_t size) {
                if (stopped() || size == 0) {
                    return;
                }
                auto appended = output_->append(data, size);
                if (!appended) {
                    error_ = appended.error();
                }
            }

            [[nodiscard]] constexpr bool stopped() const noexcept {
                return error_ != error_code::NONE;
            }

            [[nodiscard]] constexpr error_code error() const noexcept {
                return error_;
            }
        };
    }  // namespace detail

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

    template <class OutputIt, class... Args>
    [[nodiscard]] constexpr expected<OutputIt, format_error> format_to_iter_s(
        OutputIt out, std::size_t size,
        format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        detail::limited_iterator_sink<OutputIt> sink(std::move(out), size);
        detail::basic_format_context context(sink);
        detail::execute_format(context, fmt.get(), std::forward<Args>(args)...);
        return sink.take_output();
    }

    template <class Allocator, class... Args>
        requires requires { typename Allocator::value_type; } &&
                 std::is_same_v<typename Allocator::value_type, char>
    [[nodiscard]] constexpr expected<string<Allocator>, error_code> format(
        const Allocator& allocator,
        format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        auto created = string<Allocator>::try_create(allocator);
        if (!created) {
            return expected<string<Allocator>, error_code>(unexpect,
                                                           created.error());
        }

        detail::string_sink<Allocator> sink(*created);
        detail::basic_format_context context(sink);
        detail::execute_format(context, fmt.get(), std::forward<Args>(args)...);
        if (sink.stopped()) {
            return expected<string<Allocator>, error_code>(unexpect,
                                                           sink.error());
        }
        return std::move(*created);
    }

    template <class... Args>
    [[nodiscard]] constexpr expected<string<>, error_code> format(
        format_string<std::type_identity_t<Args>...> fmt, Args&&... args) {
        return tay::format(allocator<char>{}, fmt, std::forward<Args>(args)...);
    }
}  // namespace tay
