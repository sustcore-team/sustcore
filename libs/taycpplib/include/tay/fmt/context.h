/**
 * @file context.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义面向输出接收器的格式化上下文和迭代器。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/fmt/core.h>

#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>

namespace tay::detail {
    template <class Sink>
    class format_output_iterator {
    private:
        Sink* sink_ = nullptr;

        class output_proxy {
        private:
            Sink* sink_ = nullptr;

        public:
            constexpr explicit output_proxy(Sink* sink) noexcept : sink_(sink) {}

            constexpr output_proxy& operator=(char character) {
                sink_->put(character);
                return *this;
            }
        };

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type        = void;
        using difference_type   = std::ptrdiff_t;
        using pointer           = void;
        using reference         = void;

        constexpr format_output_iterator() noexcept = default;

        constexpr explicit format_output_iterator(Sink& sink) noexcept : sink_(&sink) {}

        [[nodiscard]] constexpr output_proxy operator*() const noexcept {
            return output_proxy(sink_);
        }

        constexpr format_output_iterator& operator++() noexcept {
            return *this;
        }

        constexpr format_output_iterator operator++(int) noexcept {
            return *this;
        }
    };

    template <class Sink>
    class basic_format_context {
    public:
        using iterator = format_output_iterator<Sink>;

    private:
        Sink* sink_ = nullptr;

    public:
        constexpr explicit basic_format_context(Sink& sink) noexcept : sink_(&sink) {}

        [[nodiscard]] constexpr iterator out() noexcept {
            return iterator(*sink_);
        }

        constexpr void advance_to(iterator) noexcept {}

        constexpr void put(char character) {
            sink_->put(character);
        }

        constexpr void write(const char* data, size_t size) {
            sink_->write(data, size);
        }

        constexpr void write(string_view text) {
            write(text.data(), text.size());
        }

        [[nodiscard]] constexpr bool stopped() const noexcept {
            return sink_->stopped();
        }

        template <class... Args>
        typename basic_format_context::iterator format(
            format_string<std::type_identity_t<Args>...> fmt, Args&&... args);
    };

    template <class Sink>
    basic_format_context(Sink&) -> basic_format_context<Sink>;
}  // namespace tay::detail

#include <tay/fmt/engine.h>
