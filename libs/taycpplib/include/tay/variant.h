/**
 * @file variant.h
 * @brief 提供无异常、固定存储的 tagged union。
 */

#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace tay {
    namespace detail {
        template <typename T, typename... Types>
        struct variant_index;

        template <typename T, typename... Rest>
        struct variant_index<T, T, Rest...> : std::integral_constant<size_t, 0> {};

        template <typename T, typename First, typename... Rest>
        struct variant_index<T, First, Rest...>
            : std::integral_constant<size_t, 1 + variant_index<T, Rest...>::value> {};

        template <typename T>
        struct variant_index<T>;

        template <size_t Index, typename... Types>
        struct variant_type;

        template <typename First, typename... Rest>
        struct variant_type<0, First, Rest...> {
            using type = First;
        };

        template <size_t Index, typename First, typename... Rest>
        struct variant_type<Index, First, Rest...> : variant_type<Index - 1, Rest...> {};

        template <typename Visitor, typename Variant>
        using variant_visit_result_t =
            std::invoke_result_t<Visitor &&,
                                 decltype(std::declval<Variant>()
                                              .template get<typename std::remove_cvref_t<
                                                  Variant>::template type_at<0>>())>;

        template <typename Visitor, typename Variant, size_t Index = 0>
        constexpr auto variant_visit(Visitor &&visitor,
                                     Variant &&value) -> variant_visit_result_t<Visitor, Variant> {
            using variant_type_t = std::remove_cvref_t<Variant>;
            if constexpr (Index < variant_type_t::size) {
                if (value.tag() == Index) {
                    using value_type = typename variant_type_t::template type_at<Index>;
                    return std::invoke(std::forward<Visitor>(visitor),
                                       value.template get<value_type>());
                }
                return variant_visit<Visitor, Variant, Index + 1>(std::forward<Visitor>(visitor),
                                                                  std::forward<Variant>(value));
            } else {
                __builtin_unreachable();
            }
        }
    }  // namespace detail

    template <typename... Types>
    class variant {
        static constexpr size_t max_size_ = [] {
            size_t result = 0;
            ((result = result < sizeof(Types) ? sizeof(Types) : result), ...);
            return result;
        }();
        static constexpr size_t max_align_ = [] {
            size_t result = 1;
            ((result = result < alignof(Types) ? alignof(Types) : result), ...);
            return result;
        }();

        alignas(max_align_) std::byte storage_[max_size_ == 0 ? 1 : max_size_];
        size_t tag_ = invalid_tag;

        template <size_t Index = 0>
        void destroy() noexcept {
            if constexpr (Index < size) {
                if (tag_ == Index) {
                    std::destroy_at(std::launder(reinterpret_cast<type_at<Index> *>(storage_)));
                    tag_ = invalid_tag;
                } else {
                    destroy<Index + 1>();
                }
            }
        }

        template <size_t Index = 0>
        void copy_from(const variant &other) {
            if constexpr (Index < size) {
                if (other.tag_ == Index) {
                    static_cast<void>(
                        std::construct_at(reinterpret_cast<type_at<Index> *>(storage_),
                                          other.template get<type_at<Index>>()));
                    tag_ = Index;
                } else {
                    copy_from<Index + 1>(other);
                }
            }
        }

        template <size_t Index = 0>
        void move_from(variant &&other) noexcept {
            if constexpr (Index < size) {
                if (other.tag_ == Index) {
                    static_cast<void>(
                        std::construct_at(reinterpret_cast<type_at<Index> *>(storage_),
                                          std::move(other.template get<type_at<Index>>())));
                    tag_ = Index;
                } else {
                    move_from<Index + 1>(std::move(other));
                }
            }
        }

    public:
        static constexpr size_t size        = sizeof...(Types);
        static constexpr size_t invalid_tag = static_cast<size_t>(-1);

        template <size_t Index>
        using type_at = typename detail::variant_type<Index, Types...>::type;

        constexpr variant() noexcept = default;

        template <typename T>
            requires((std::same_as<std::remove_cvref_t<T>, Types> || ...))
        constexpr variant(T &&value) noexcept(
            std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T &&>) {
            using value_type       = std::remove_cvref_t<T>;
            constexpr size_t index = detail::variant_index<value_type, Types...>::value;
            static_cast<void>(std::construct_at(reinterpret_cast<value_type *>(storage_),
                                                std::forward<T>(value)));
            tag_ = index;
        }

        variant(const variant &other) {
            if (other)
                copy_from(other);
        }

        variant(variant &&other) noexcept {
            if (other)
                move_from(std::move(other));
        }

        ~variant() noexcept {
            destroy();
        }

        variant &operator=(const variant &other) {
            if (this != &other) {
                destroy();
                if (other)
                    copy_from(other);
            }
            return *this;
        }

        variant &operator=(variant &&other) noexcept {
            if (this != &other) {
                destroy();
                if (other)
                    move_from(std::move(other));
            }
            return *this;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return tag_ != invalid_tag;
        }

        [[nodiscard]] constexpr size_t tag() const noexcept {
            return tag_;
        }

        template <typename T>
        [[nodiscard]] constexpr bool is() const noexcept {
            return tag_ == detail::variant_index<T, Types...>::value;
        }

        template <typename T>
        [[nodiscard]] T &get() noexcept {
            return *std::launder(reinterpret_cast<T *>(storage_));
        }

        template <typename T>
        [[nodiscard]] const T &get() const noexcept {
            return *std::launder(reinterpret_cast<const T *>(storage_));
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) {
            return detail::variant_visit<Visitor, variant &>(std::forward<Visitor>(visitor), *this);
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor &&visitor) const {
            return detail::variant_visit<Visitor, const variant &>(std::forward<Visitor>(visitor),
                                                                   *this);
        }
    };

    template <typename Visitor, typename... Types>
    constexpr decltype(auto) visit(Visitor &&visitor, variant<Types...> &value) {
        return value.visit(std::forward<Visitor>(visitor));
    }

    template <typename Visitor, typename... Types>
    constexpr decltype(auto) visit(Visitor &&visitor, const variant<Types...> &value) {
        return value.visit(std::forward<Visitor>(visitor));
    }
}  // namespace tay
