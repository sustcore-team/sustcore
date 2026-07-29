/**
 * @file context.h
 * @brief Sink-backed formatting context and output iterator.
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
            constexpr explicit output_proxy(Sink* sink) noexcept
                : sink_(sink) {}

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

        constexpr explicit format_output_iterator(Sink& sink) noexcept
            : sink_(&sink) {}

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
        constexpr explicit basic_format_context(Sink& sink) noexcept
            : sink_(&sink) {}

        [[nodiscard]] constexpr iterator out() noexcept {
            return iterator(*sink_);
        }

        constexpr void advance_to(iterator) noexcept {}

        constexpr void put(char character) {
            sink_->put(character);
        }

        constexpr void write(const char* data, std::size_t size) {
            sink_->write(data, size);
        }

        constexpr void write(string_view text) {
            write(text.data(), text.size());
        }

        template <class As = infer_format_type, class Value>
        typename basic_format_context::iterator format(Value&& value) {
            using selected_type = std::conditional_t<
                std::is_same_v<As, infer_format_type>, format_arg_t<Value>, As>;

            formatter<selected_type> selected{};
            format_parse_context parse_context(string_view{});
            (void)selected.parse(parse_context);
            return selected.format(std::forward<Value>(value), *this);
        }
    };

    template <class Sink>
    basic_format_context(Sink&) -> basic_format_context<Sink>;
}  // namespace tay::detail
