/**
 * @file expected.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供支持左值引用的无异常值或错误结果类型。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <tay/in_place.h>
#include <tay/panic.h>

#include <concepts>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace tay {
    struct unexpect_t {
        explicit constexpr unexpect_t() = default;
    };

    inline constexpr unexpect_t unexpect{};

    struct try_in_place_t {
        explicit constexpr try_in_place_t() = default;
    };

    inline constexpr try_in_place_t try_in_place{};

    template <typename E>
    class unexpected;

    template <typename T, typename E>
    class expected;

    namespace detail {
        template <typename T>
        inline constexpr bool valid_error_type_v =
            std::is_object_v<T> && !std::is_array_v<T> && std::is_same_v<T, std::remove_cv_t<T>>;

        template <typename T>
        inline constexpr bool valid_owned_value_type_v = std::is_object_v<T> && !std::is_array_v<T>;

        template <typename T>
        struct is_unexpected : std::false_type {};

        template <typename E>
        struct is_unexpected<unexpected<E>> : std::true_type {};

        template <typename T>
        inline constexpr bool is_unexpected_v = is_unexpected<std::remove_cvref_t<T>>::value;

        template <typename T>
        struct is_expected : std::false_type {};

        template <typename V, typename E>
        struct is_expected<expected<V, E>> : std::true_type {};

        template <typename T>
        inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

        template <typename T>
        struct is_ok_wrapper : std::false_type {};

        template <typename T, typename... Args>
        constexpr T* construct_at(T* location, Args&&... args) {
            return std::construct_at(location, std::forward<Args>(args)...);
        }

        template <typename T>
        constexpr void destroy_at(T* location) noexcept {
            std::destroy_at(location);
        }

        template <typename V, typename E>
        union storage {
            char empty;
            V value;
            E error;

            constexpr storage() noexcept : empty{} {}
            constexpr ~storage() {}
        };

        template <typename New, typename Old, typename... Args>
        inline constexpr bool can_reinit_v = std::is_constructible_v<New, Args...> &&
                                             (std::is_nothrow_constructible_v<New, Args...> ||
                                              std::is_nothrow_move_constructible_v<New> ||
                                              std::is_nothrow_move_constructible_v<Old>);

        template <typename New, typename Old, typename... Args>
            requires(can_reinit_v<New, Old, Args...>)
        constexpr void reinit(New* new_location, Old* old_location, Args&&... args) {
            if constexpr (std::is_nothrow_constructible_v<New, Args...>) {
                destroy_at(old_location);
                construct_at(new_location, std::forward<Args>(args)...);
            } else if constexpr (std::is_nothrow_move_constructible_v<New>) {
                New temporary(std::forward<Args>(args)...);
                destroy_at(old_location);
                construct_at(new_location, std::move(temporary));
            } else {
                static_assert(std::is_nothrow_move_constructible_v<Old>);
                Old temporary(std::move(*old_location));
                destroy_at(old_location);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
                    construct_at(new_location, std::forward<Args>(args)...);
                } catch (...) {
                    construct_at(old_location, std::move(temporary));
                    throw;
                }
#else
                construct_at(new_location, std::forward<Args>(args)...);
#endif
            }
        }

        template <typename R>
        struct transform_value {
            using type = std::remove_cvref_t<R>;
        };

        template <typename R>
        struct transform_value<R&> {
            using type = R&;
        };

        template <typename R>
        struct transform_value<R&&> {
            using type = std::remove_cvref_t<R>;
        };

        template <>
        struct transform_value<void> {
            using type = void;
        };

        template <typename R>
        using transform_value_t = typename transform_value<R>::type;

        template <typename Self, typename F>
        constexpr auto and_then_impl(Self&& self, F&& function) {
            using source_type = std::remove_cvref_t<Self>;
            using error_type  = typename source_type::error_type;

            if constexpr (std::is_void_v<typename source_type::value_type>) {
                using raw_result  = decltype(std::invoke(std::forward<F>(function)));
                using result_type = std::remove_cvref_t<raw_result>;
                static_assert(is_expected_v<result_type>,
                              "and_then callback must return tay::expected");
                static_assert(std::is_same_v<typename result_type::error_type, error_type>,
                              "and_then callback must preserve the error type");
                if (self.has_value()) {
                    return std::invoke(std::forward<F>(function));
                }
                return result_type(unexpect, std::forward<Self>(self).error());
            } else {
                using raw_result =
                    decltype(std::invoke(std::forward<F>(function), *std::forward<Self>(self)));
                using result_type = std::remove_cvref_t<raw_result>;
                static_assert(is_expected_v<result_type>,
                              "and_then callback must return tay::expected");
                static_assert(std::is_same_v<typename result_type::error_type, error_type>,
                              "and_then callback must preserve the error type");
                if (self.has_value()) {
                    return std::invoke(std::forward<F>(function), *std::forward<Self>(self));
                }
                return result_type(unexpect, std::forward<Self>(self).error());
            }
        }

        template <typename Self, typename F>
        constexpr auto transform_impl(Self&& self, F&& function) {
            using source_type = std::remove_cvref_t<Self>;
            using error_type  = typename source_type::error_type;

            if constexpr (std::is_void_v<typename source_type::value_type>) {
                using raw_result  = decltype(std::invoke(std::forward<F>(function)));
                using value_type  = transform_value_t<raw_result>;
                using result_type = expected<value_type, error_type>;
                if (!self.has_value()) {
                    return result_type(unexpect, std::forward<Self>(self).error());
                }
                if constexpr (std::is_void_v<raw_result>) {
                    std::invoke(std::forward<F>(function));
                    return result_type{};
                } else {
                    return result_type(std::invoke(std::forward<F>(function)));
                }
            } else {
                using raw_result =
                    decltype(std::invoke(std::forward<F>(function), *std::forward<Self>(self)));
                using value_type  = transform_value_t<raw_result>;
                using result_type = expected<value_type, error_type>;
                if (!self.has_value()) {
                    return result_type(unexpect, std::forward<Self>(self).error());
                }
                if constexpr (std::is_void_v<raw_result>) {
                    std::invoke(std::forward<F>(function), *std::forward<Self>(self));
                    return result_type{};
                } else {
                    return result_type(
                        std::invoke(std::forward<F>(function), *std::forward<Self>(self)));
                }
            }
        }

        template <typename Self, typename F>
        constexpr auto or_else_impl(Self&& self, F&& function) {
            using source_type = std::remove_cvref_t<Self>;
            using value_type  = typename source_type::value_type;
            using raw_result =
                decltype(std::invoke(std::forward<F>(function), std::forward<Self>(self).error()));
            using result_type = std::remove_cvref_t<raw_result>;
            static_assert(is_expected_v<result_type>, "or_else callback must return tay::expected");
            static_assert(std::is_same_v<typename result_type::value_type, value_type>,
                          "or_else callback must preserve the value type");

            if (!self.has_value()) {
                return std::invoke(std::forward<F>(function), std::forward<Self>(self).error());
            }
            if constexpr (std::is_void_v<value_type>) {
                return result_type{};
            } else {
                return result_type(*std::forward<Self>(self));
            }
        }

        template <typename Self, typename F>
        constexpr auto transform_error_impl(Self&& self, F&& function) {
            using source_type = std::remove_cvref_t<Self>;
            using value_type  = typename source_type::value_type;
            using raw_error =
                decltype(std::invoke(std::forward<F>(function), std::forward<Self>(self).error()));
            static_assert(!std::is_void_v<raw_error> && !std::is_reference_v<raw_error>,
                          "transform_error callback must return an object");
            using error_type = std::remove_cvref_t<raw_error>;
            static_assert(valid_error_type_v<error_type>,
                          "transform_error callback returned an invalid error type");
            using result_type = expected<value_type, error_type>;

            if (!self.has_value()) {
                return result_type(unexpect, std::invoke(std::forward<F>(function),
                                                         std::forward<Self>(self).error()));
            }
            if constexpr (std::is_void_v<value_type>) {
                return result_type{};
            } else {
                return result_type(*std::forward<Self>(self));
            }
        }

        template <typename Self, typename Visitor>
        constexpr decltype(auto) match_impl(Self&& self, Visitor&& visitor) {
            using source_type = std::remove_cvref_t<Self>;
            if (self.has_value()) {
                if constexpr (std::is_void_v<typename source_type::value_type>) {
                    return std::invoke(std::forward<Visitor>(visitor));
                } else {
                    return std::invoke(std::forward<Visitor>(visitor), *std::forward<Self>(self));
                }
            }
            return std::invoke(std::forward<Visitor>(visitor), std::forward<Self>(self).error());
        }
    }  // namespace detail

    template <typename E>
    class unexpected {
        static_assert(detail::valid_error_type_v<E>,
                      "unexpected<E> requires an unqualified non-array object");

        E error_;

    public:
        using error_type = E;

        constexpr unexpected(const unexpected&)            = default;
        constexpr unexpected(unexpected&&)                 = default;
        constexpr unexpected& operator=(const unexpected&) = default;
        constexpr unexpected& operator=(unexpected&&)      = default;
        constexpr ~unexpected()                            = default;

        template <typename G = E>
            requires(!detail::is_unexpected_v<G> &&
                     !std::is_same_v<std::remove_cvref_t<G>, unexpect_t> &&
                     std::is_constructible_v<E, G &&>)
        constexpr explicit(!std::is_convertible_v<G&&, E>)
            unexpected(G&& error) noexcept(std::is_nothrow_constructible_v<E, G&&>)
            : error_(std::forward<G>(error)) {}

        template <typename... Args>
            requires std::is_constructible_v<E, Args&&...>
        constexpr explicit unexpected(unexpect_t, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<E, Args&&...>)
            : error_(std::forward<Args>(args)...) {}

        [[nodiscard]] constexpr E& error() & noexcept {
            return error_;
        }
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return error_;
        }
        [[nodiscard]] constexpr E&& error() && noexcept {
            return std::move(error_);
        }
        [[nodiscard]] constexpr const E&& error() const&& noexcept {
            return std::move(error_);
        }

        constexpr void swap(unexpected& other) noexcept(noexcept(std::swap(error_, other.error_)))
            requires requires { std::swap(error_, other.error_); }
        {
            std::swap(error_, other.error_);
        }
    };

    template <typename E>
    unexpected(E) -> unexpected<E>;

    template <typename T, typename E>
    class expected {
        static_assert(detail::valid_owned_value_type_v<T>,
                      "expected<T, E> requires a non-array object value type");
        static_assert(detail::valid_error_type_v<E>,
                      "expected<T, E> requires an unqualified non-array error object");

        detail::storage<T, E> storage_;
        bool has_value_;

        constexpr void destroy_active() noexcept {
            if (has_value_) {
                detail::destroy_at(std::addressof(storage_.value));
            } else {
                detail::destroy_at(std::addressof(storage_.error));
            }
        }

        template <typename U>
        static constexpr bool valid_value_argument =
            !std::is_same_v<std::remove_cvref_t<U>, expected> && !detail::is_unexpected_v<U> &&
            !detail::is_ok_wrapper<std::remove_cvref_t<U>>::value &&
            !std::is_same_v<std::remove_cvref_t<U>, unexpect_t>;

    public:
        using value_type = T;
        using error_type = E;

        constexpr expected() noexcept(std::is_nothrow_default_constructible_v<T>)
            requires std::is_default_constructible_v<T>
            : storage_{}, has_value_(true) {
            detail::construct_at(std::addressof(storage_.value));
        }

        template <typename... Args>
            requires std::is_constructible_v<T, Args&&...>
        constexpr explicit expected(in_place_t, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<T, Args&&...>)
            : storage_{}, has_value_(true) {
            detail::construct_at(std::addressof(storage_.value), std::forward<Args>(args)...);
        }

        template <typename initializer_t, typename... Args>
            requires(std::is_nothrow_constructible_v<T, Args && ...> &&
                     requires(initializer_t&& initializer, T& value) {
                         {
                             std::forward<initializer_t>(initializer)(value)
                         } -> std::same_as<expected<void, E>>;
                     })
        constexpr explicit expected(try_in_place_t, initializer_t&& initializer,
                                    Args&&... args) noexcept
            : storage_{}, has_value_(true) {
            detail::construct_at(std::addressof(storage_.value), std::forward<Args>(args)...);
            auto initialized = std::forward<initializer_t>(initializer)(storage_.value);
            if (!initialized) {
                E error = std::move(initialized).error();
                detail::destroy_at(std::addressof(storage_.value));
                detail::construct_at(std::addressof(storage_.error), std::move(error));
                has_value_ = false;
            }
        }

        template <typename U = T>
            requires(valid_value_argument<U> && std::is_constructible_v<T, U &&>)
        constexpr explicit(!std::is_convertible_v<U&&, T>)
            expected(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
            : storage_{}, has_value_(true) {
            detail::construct_at(std::addressof(storage_.value), std::forward<U>(value));
        }

        template <typename G>
            requires std::is_constructible_v<E, const G&>
        constexpr explicit(!std::is_convertible_v<const G&, E>) expected(
            const unexpected<G>& error) noexcept(std::is_nothrow_constructible_v<E, const G&>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), error.error());
        }

        template <typename G>
            requires std::is_constructible_v<E, G&&>
        constexpr explicit(!std::is_convertible_v<G&&, E>)
            expected(unexpected<G>&& error) noexcept(std::is_nothrow_constructible_v<E, G&&>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), std::move(error).error());
        }

        template <typename... Args>
            requires std::is_constructible_v<E, Args&&...>
        constexpr explicit expected(unexpect_t, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<E, Args&&...>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), std::forward<Args>(args)...);
        }

        constexpr expected(const expected& other) noexcept(
            std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E>)
            requires(std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>)
            : storage_{}, has_value_(other.has_value_) {
            if (has_value_) {
                detail::construct_at(std::addressof(storage_.value), other.storage_.value);
            } else {
                detail::construct_at(std::addressof(storage_.error), other.storage_.error);
            }
        }

        constexpr expected(const expected&)
            requires(!std::is_copy_constructible_v<T> || !std::is_copy_constructible_v<E>)
        = delete;

        constexpr expected(expected&& other) noexcept(std::is_nothrow_move_constructible_v<T> &&
                                                      std::is_nothrow_move_constructible_v<E>)
            requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E>)
            : storage_{}, has_value_(other.has_value_) {
            if (has_value_) {
                detail::construct_at(std::addressof(storage_.value),
                                     std::move(other.storage_.value));
            } else {
                detail::construct_at(std::addressof(storage_.error),
                                     std::move(other.storage_.error));
            }
        }

        constexpr expected(expected&&)
            requires(!std::is_move_constructible_v<T> || !std::is_move_constructible_v<E>)
        = delete;

        constexpr ~expected() {
            destroy_active();
        }

        constexpr expected& operator=(const expected& other)
            requires(std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T> &&
                     std::is_copy_constructible_v<E> && std::is_copy_assignable_v<E> &&
                     detail::can_reinit_v<T, E, const T&> && detail::can_reinit_v<E, T, const E&>)
        {
            if (this == std::addressof(other)) {
                return *this;
            }
            if (has_value_ && other.has_value_) {
                storage_.value = other.storage_.value;
            } else if (!has_value_ && !other.has_value_) {
                storage_.error = other.storage_.error;
            } else if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               other.storage_.error);
                has_value_ = false;
            } else {
                detail::reinit(std::addressof(storage_.value), std::addressof(storage_.error),
                               other.storage_.value);
                has_value_ = true;
            }
            return *this;
        }

        constexpr expected& operator=(const expected&)
            requires(!std::is_copy_constructible_v<T> || !std::is_copy_assignable_v<T> ||
                     !std::is_copy_constructible_v<E> || !std::is_copy_assignable_v<E> ||
                     !detail::can_reinit_v<T, E, const T&> || !detail::can_reinit_v<E, T, const E&>)
        = delete;

        constexpr expected& operator=(expected&& other) noexcept(
            std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<E> &&
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
            requires(std::is_move_constructible_v<T> && std::is_move_assignable_v<T> &&
                     std::is_move_constructible_v<E> && std::is_move_assignable_v<E> &&
                     detail::can_reinit_v<T, E, T &&> && detail::can_reinit_v<E, T, E &&>)
        {
            if (this == std::addressof(other)) {
                return *this;
            }
            if (has_value_ && other.has_value_) {
                storage_.value = std::move(other.storage_.value);
            } else if (!has_value_ && !other.has_value_) {
                storage_.error = std::move(other.storage_.error);
            } else if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               std::move(other.storage_.error));
                has_value_ = false;
            } else {
                detail::reinit(std::addressof(storage_.value), std::addressof(storage_.error),
                               std::move(other.storage_.value));
                has_value_ = true;
            }
            return *this;
        }

        constexpr expected& operator=(expected&&)
            requires(!std::is_move_constructible_v<T> || !std::is_move_assignable_v<T> ||
                     !std::is_move_constructible_v<E> || !std::is_move_assignable_v<E> ||
                     !detail::can_reinit_v<T, E, T &&> || !detail::can_reinit_v<E, T, E &&>)
        = delete;

        template <typename U = T>
            requires(valid_value_argument<U> && std::is_constructible_v<T, U &&> &&
                     std::is_assignable_v<T&, U &&> && detail::can_reinit_v<T, E, U &&>)
        constexpr expected& operator=(U&& value) {
            if (has_value_) {
                storage_.value = std::forward<U>(value);
            } else {
                detail::reinit(std::addressof(storage_.value), std::addressof(storage_.error),
                               std::forward<U>(value));
                has_value_ = true;
            }
            return *this;
        }

        template <typename G>
            requires(std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&> &&
                     detail::can_reinit_v<E, T, const G&>)
        constexpr expected& operator=(const unexpected<G>& error) {
            if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               error.error());
                has_value_ = false;
            } else {
                storage_.error = error.error();
            }
            return *this;
        }

        template <typename G>
            requires(std::is_constructible_v<E, G &&> && std::is_assignable_v<E&, G &&> &&
                     detail::can_reinit_v<E, T, G &&>)
        constexpr expected& operator=(unexpected<G>&& error) {
            if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               std::move(error).error());
                has_value_ = false;
            } else {
                storage_.error = std::move(error).error();
            }
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }
        constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        [[nodiscard]] constexpr T* operator->() noexcept {
            return std::addressof(storage_.value);
        }
        [[nodiscard]] constexpr const T* operator->() const noexcept {
            return std::addressof(storage_.value);
        }
        [[nodiscard]] constexpr T& operator*() & noexcept {
            return storage_.value;
        }
        [[nodiscard]] constexpr const T& operator*() const& noexcept {
            return storage_.value;
        }
        [[nodiscard]] constexpr T&& operator*() && noexcept {
            return std::move(storage_.value);
        }
        [[nodiscard]] constexpr const T&& operator*() const&& noexcept {
            return std::move(storage_.value);
        }

        [[nodiscard]] constexpr T& value() & {
            if (!has_value_) {
                tay::panic("bad expected access");
            }
            return storage_.value;
        }
        [[nodiscard]] constexpr const T& value() const& {
            if (!has_value_) {
                tay::panic("bad expected access");
            }
            return storage_.value;
        }
        [[nodiscard]] constexpr T&& value() && {
            if (!has_value_) {
                tay::panic("bad expected access");
            }
            return std::move(storage_.value);
        }
        [[nodiscard]] constexpr const T&& value() const&& {
            if (!has_value_) {
                tay::panic("bad expected access");
            }
            return std::move(storage_.value);
        }

        [[nodiscard]] constexpr E& error() & noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr E&& error() && noexcept {
            return std::move(storage_.error);
        }
        [[nodiscard]] constexpr const E&& error() const&& noexcept {
            return std::move(storage_.error);
        }

        template <typename... Args>
            requires std::is_nothrow_constructible_v<T, Args&&...>
        constexpr T& emplace(Args&&... args) noexcept {
            destroy_active();
            detail::construct_at(std::addressof(storage_.value), std::forward<Args>(args)...);
            has_value_ = true;
            return storage_.value;
        }

        constexpr void swap(expected& other) noexcept(
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E> &&
            noexcept(std::swap(storage_.value, other.storage_.value)) &&
            noexcept(std::swap(storage_.error, other.storage_.error)))
            requires(std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
                     (std::is_nothrow_move_constructible_v<T> ||
                      std::is_nothrow_move_constructible_v<E>) &&
                     requires {
                         std::swap(storage_.value, other.storage_.value);
                         std::swap(storage_.error, other.storage_.error);
                     })
        {
            if (has_value_ && other.has_value_) {
                std::swap(storage_.value, other.storage_.value);
                return;
            }
            if (!has_value_ && !other.has_value_) {
                std::swap(storage_.error, other.storage_.error);
                return;
            }
            expected* value_side = has_value_ ? this : std::addressof(other);
            expected* error_side = has_value_ ? std::addressof(other) : this;

            if constexpr (std::is_nothrow_move_constructible_v<E>) {
                E temporary(std::move(error_side->storage_.error));
                detail::destroy_at(std::addressof(error_side->storage_.error));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
                    detail::construct_at(std::addressof(error_side->storage_.value),
                                         std::move(value_side->storage_.value));
                } catch (...) {
                    detail::construct_at(std::addressof(error_side->storage_.error),
                                         std::move(temporary));
                    throw;
                }
#else
                detail::construct_at(std::addressof(error_side->storage_.value),
                                     std::move(value_side->storage_.value));
#endif
                detail::destroy_at(std::addressof(value_side->storage_.value));
                detail::construct_at(std::addressof(value_side->storage_.error),
                                     std::move(temporary));
            } else {
                static_assert(std::is_nothrow_move_constructible_v<T>);
                T temporary(std::move(value_side->storage_.value));
                detail::destroy_at(std::addressof(value_side->storage_.value));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
                try {
                    detail::construct_at(std::addressof(value_side->storage_.error),
                                         std::move(error_side->storage_.error));
                } catch (...) {
                    detail::construct_at(std::addressof(value_side->storage_.value),
                                         std::move(temporary));
                    throw;
                }
#else
                detail::construct_at(std::addressof(value_side->storage_.error),
                                     std::move(error_side->storage_.error));
#endif
                detail::destroy_at(std::addressof(error_side->storage_.error));
                detail::construct_at(std::addressof(error_side->storage_.value),
                                     std::move(temporary));
            }
            value_side->has_value_ = false;
            error_side->has_value_ = true;
        }

        template <typename F>
        constexpr auto and_then(F&& function) & {
            return detail::and_then_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) const& {
            return detail::and_then_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) && {
            return detail::and_then_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) const&& {
            return detail::and_then_impl(std::move(*this), std::forward<F>(function));
        }

        template <typename F>
        constexpr auto transform(F&& function) & {
            return detail::transform_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) const& {
            return detail::transform_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) && {
            return detail::transform_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) const&& {
            return detail::transform_impl(std::move(*this), std::forward<F>(function));
        }

        template <typename F>
        constexpr auto or_else(F&& function) & {
            return detail::or_else_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) const& {
            return detail::or_else_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) && {
            return detail::or_else_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) const&& {
            return detail::or_else_impl(std::move(*this), std::forward<F>(function));
        }

        template <typename F>
        constexpr auto transform_error(F&& function) & {
            return detail::transform_error_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) const& {
            return detail::transform_error_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) && {
            return detail::transform_error_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) const&& {
            return detail::transform_error_impl(std::move(*this), std::forward<F>(function));
        }

        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) & {
            return detail::match_impl(*this, std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) const& {
            return detail::match_impl(*this, std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) && {
            return detail::match_impl(std::move(*this), std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) const&& {
            return detail::match_impl(std::move(*this), std::forward<Visitor>(visitor));
        }

        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) & {
            return match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) const& {
            return match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) && {
            return std::move(*this).match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) const&& {
            return std::move(*this).match(std::forward<Visitor>(visitor));
        }
    };

    template <typename E>
    class expected<void, E> {
        static_assert(detail::valid_error_type_v<E>,
                      "expected<void, E> requires an unqualified non-array error object");

        detail::storage<char, E> storage_;
        bool has_value_;

        constexpr void destroy_active() noexcept {
            if (has_value_) {
                detail::destroy_at(std::addressof(storage_.value));
            } else {
                detail::destroy_at(std::addressof(storage_.error));
            }
        }

    public:
        using value_type = void;
        using error_type = E;

        constexpr expected() noexcept : storage_{}, has_value_(true) {
            detail::construct_at(std::addressof(storage_.value), char{});
        }

        template <typename G>
            requires std::is_constructible_v<E, const G&>
        constexpr explicit(!std::is_convertible_v<const G&, E>) expected(
            const unexpected<G>& error) noexcept(std::is_nothrow_constructible_v<E, const G&>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), error.error());
        }

        template <typename G>
            requires std::is_constructible_v<E, G&&>
        constexpr explicit(!std::is_convertible_v<G&&, E>)
            expected(unexpected<G>&& error) noexcept(std::is_nothrow_constructible_v<E, G&&>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), std::move(error).error());
        }

        template <typename... Args>
            requires std::is_constructible_v<E, Args&&...>
        constexpr explicit expected(unexpect_t, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<E, Args&&...>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), std::forward<Args>(args)...);
        }

        constexpr expected(const expected& other) noexcept(std::is_nothrow_copy_constructible_v<E>)
            requires std::is_copy_constructible_v<E>
            : storage_{}, has_value_(other.has_value_) {
            if (has_value_) {
                detail::construct_at(std::addressof(storage_.value), char{});
            } else {
                detail::construct_at(std::addressof(storage_.error), other.storage_.error);
            }
        }

        constexpr expected(const expected&)
            requires(!std::is_copy_constructible_v<E>)
        = delete;

        constexpr expected(expected&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
            requires std::is_move_constructible_v<E>
            : storage_{}, has_value_(other.has_value_) {
            if (has_value_) {
                detail::construct_at(std::addressof(storage_.value), char{});
            } else {
                detail::construct_at(std::addressof(storage_.error),
                                     std::move(other.storage_.error));
            }
        }

        constexpr expected(expected&&)
            requires(!std::is_move_constructible_v<E>)
        = delete;

        constexpr ~expected() {
            destroy_active();
        }

        constexpr expected& operator=(const expected& other)
            requires(std::is_copy_constructible_v<E> && std::is_copy_assignable_v<E>)
        {
            if (this == std::addressof(other)) {
                return *this;
            }
            if (has_value_ && other.has_value_) {
                return *this;
            }
            if (!has_value_ && !other.has_value_) {
                storage_.error = other.storage_.error;
            } else if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               other.storage_.error);
                has_value_ = false;
            } else {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), char{});
                has_value_ = true;
            }
            return *this;
        }

        constexpr expected& operator=(const expected&)
            requires(!std::is_copy_constructible_v<E> || !std::is_copy_assignable_v<E>)
        = delete;

        constexpr expected& operator=(expected&& other) noexcept(
            std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<E>)
            requires(std::is_move_constructible_v<E> && std::is_move_assignable_v<E>)
        {
            if (this == std::addressof(other)) {
                return *this;
            }
            if (has_value_ && other.has_value_) {
                return *this;
            }
            if (!has_value_ && !other.has_value_) {
                storage_.error = std::move(other.storage_.error);
            } else if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               std::move(other.storage_.error));
                has_value_ = false;
            } else {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), char{});
                has_value_ = true;
            }
            return *this;
        }

        constexpr expected& operator=(expected&&)
            requires(!std::is_move_constructible_v<E> || !std::is_move_assignable_v<E>)
        = delete;

        template <typename G>
            requires(std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>)
        constexpr expected& operator=(const unexpected<G>& error) {
            if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               error.error());
                has_value_ = false;
            } else {
                storage_.error = error.error();
            }
            return *this;
        }

        template <typename G>
            requires(std::is_constructible_v<E, G &&> && std::is_assignable_v<E&, G &&>)
        constexpr expected& operator=(unexpected<G>&& error) {
            if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               std::move(error).error());
                has_value_ = false;
            } else {
                storage_.error = std::move(error).error();
            }
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }
        constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        constexpr void value() const {
            if (!has_value_) {
                tay::panic("bad expected access");
            }
        }

        [[nodiscard]] constexpr E& error() & noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr E&& error() && noexcept {
            return std::move(storage_.error);
        }
        [[nodiscard]] constexpr const E&& error() const&& noexcept {
            return std::move(storage_.error);
        }

        constexpr void emplace() noexcept {
            if (!has_value_) {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), char{});
                has_value_ = true;
            }
        }

        constexpr void swap(expected& other) noexcept(std::is_nothrow_move_constructible_v<E> &&
                                                      noexcept(std::swap(storage_.error,
                                                                         other.storage_.error)))
            requires(std::is_move_constructible_v<E> &&
                     requires { std::swap(storage_.error, other.storage_.error); })
        {
            if (has_value_ && other.has_value_) {
                return;
            }
            if (!has_value_ && !other.has_value_) {
                std::swap(storage_.error, other.storage_.error);
                return;
            }
            expected* value_side = has_value_ ? this : std::addressof(other);
            expected* error_side = has_value_ ? std::addressof(other) : this;
            detail::destroy_at(std::addressof(value_side->storage_.value));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                detail::construct_at(std::addressof(value_side->storage_.error),
                                     std::move(error_side->storage_.error));
            } catch (...) {
                detail::construct_at(std::addressof(value_side->storage_.value), char{});
                throw;
            }
#else
            detail::construct_at(std::addressof(value_side->storage_.error),
                                 std::move(error_side->storage_.error));
#endif
            detail::destroy_at(std::addressof(error_side->storage_.error));
            detail::construct_at(std::addressof(error_side->storage_.value), char{});
            value_side->has_value_ = false;
            error_side->has_value_ = true;
        }

        template <typename F>
        constexpr auto and_then(F&& function) & {
            return detail::and_then_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) const& {
            return detail::and_then_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) && {
            return detail::and_then_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) const&& {
            return detail::and_then_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) & {
            return detail::transform_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) const& {
            return detail::transform_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) && {
            return detail::transform_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) const&& {
            return detail::transform_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) & {
            return detail::or_else_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) const& {
            return detail::or_else_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) && {
            return detail::or_else_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) const&& {
            return detail::or_else_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) & {
            return detail::transform_error_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) const& {
            return detail::transform_error_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) && {
            return detail::transform_error_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) const&& {
            return detail::transform_error_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) & {
            return detail::match_impl(*this, std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) const& {
            return detail::match_impl(*this, std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) && {
            return detail::match_impl(std::move(*this), std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) const&& {
            return detail::match_impl(std::move(*this), std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) & {
            return match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) const& {
            return match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) && {
            return std::move(*this).match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) const&& {
            return std::move(*this).match(std::forward<Visitor>(visitor));
        }
    };

    template <typename T, typename E>
    class expected<T&, E> {
        static_assert(detail::valid_error_type_v<E>,
                      "expected<T&, E> requires an unqualified non-array error object");

        detail::storage<T*, E> storage_;
        bool has_value_;

        constexpr void destroy_active() noexcept {
            if (has_value_) {
                detail::destroy_at(std::addressof(storage_.value));
            } else {
                detail::destroy_at(std::addressof(storage_.error));
            }
        }

    public:
        using value_type = T&;
        using error_type = E;

        expected() = delete;

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr expected(U& value) noexcept : storage_{}, has_value_(true) {
            detail::construct_at(std::addressof(storage_.value), std::addressof(value));
        }

        template <typename G>
            requires std::is_constructible_v<E, const G&>
        constexpr explicit(!std::is_convertible_v<const G&, E>) expected(
            const unexpected<G>& error) noexcept(std::is_nothrow_constructible_v<E, const G&>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), error.error());
        }

        template <typename G>
            requires std::is_constructible_v<E, G&&>
        constexpr explicit(!std::is_convertible_v<G&&, E>)
            expected(unexpected<G>&& error) noexcept(std::is_nothrow_constructible_v<E, G&&>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), std::move(error).error());
        }

        template <typename... Args>
            requires std::is_constructible_v<E, Args&&...>
        constexpr explicit expected(unexpect_t, Args&&... args) noexcept(
            std::is_nothrow_constructible_v<E, Args&&...>)
            : storage_{}, has_value_(false) {
            detail::construct_at(std::addressof(storage_.error), std::forward<Args>(args)...);
        }

        constexpr expected(const expected& other) noexcept(std::is_nothrow_copy_constructible_v<E>)
            requires std::is_copy_constructible_v<E>
            : storage_{}, has_value_(other.has_value_) {
            if (has_value_) {
                detail::construct_at(std::addressof(storage_.value), other.storage_.value);
            } else {
                detail::construct_at(std::addressof(storage_.error), other.storage_.error);
            }
        }

        constexpr expected(const expected&)
            requires(!std::is_copy_constructible_v<E>)
        = delete;

        constexpr expected(expected&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
            requires std::is_move_constructible_v<E>
            : storage_{}, has_value_(other.has_value_) {
            if (has_value_) {
                detail::construct_at(std::addressof(storage_.value), other.storage_.value);
            } else {
                detail::construct_at(std::addressof(storage_.error),
                                     std::move(other.storage_.error));
            }
        }

        constexpr expected(expected&&)
            requires(!std::is_move_constructible_v<E>)
        = delete;

        constexpr ~expected() {
            destroy_active();
        }

        constexpr expected& operator=(const expected& other)
            requires(std::is_copy_constructible_v<E> && std::is_copy_assignable_v<E>)
        {
            if (this == std::addressof(other)) {
                return *this;
            }
            if (has_value_ && other.has_value_) {
                storage_.value = other.storage_.value;
            } else if (!has_value_ && !other.has_value_) {
                storage_.error = other.storage_.error;
            } else if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               other.storage_.error);
                has_value_ = false;
            } else {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), other.storage_.value);
                has_value_ = true;
            }
            return *this;
        }

        constexpr expected& operator=(const expected&)
            requires(!std::is_copy_constructible_v<E> || !std::is_copy_assignable_v<E>)
        = delete;

        constexpr expected& operator=(expected&& other) noexcept(
            std::is_nothrow_move_constructible_v<E> && std::is_nothrow_move_assignable_v<E>)
            requires(std::is_move_constructible_v<E> && std::is_move_assignable_v<E>)
        {
            if (this == std::addressof(other)) {
                return *this;
            }
            if (has_value_ && other.has_value_) {
                storage_.value = other.storage_.value;
            } else if (!has_value_ && !other.has_value_) {
                storage_.error = std::move(other.storage_.error);
            } else if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               std::move(other.storage_.error));
                has_value_ = false;
            } else {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), other.storage_.value);
                has_value_ = true;
            }
            return *this;
        }

        constexpr expected& operator=(expected&&)
            requires(!std::is_move_constructible_v<E> || !std::is_move_assignable_v<E>)
        = delete;

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr expected& operator=(U& value) noexcept {
            if (has_value_) {
                storage_.value = std::addressof(value);
            } else {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), std::addressof(value));
                has_value_ = true;
            }
            return *this;
        }

        template <typename G>
            requires(std::is_constructible_v<E, const G&> && std::is_assignable_v<E&, const G&>)
        constexpr expected& operator=(const unexpected<G>& error) {
            if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               error.error());
                has_value_ = false;
            } else {
                storage_.error = error.error();
            }
            return *this;
        }

        template <typename G>
            requires(std::is_constructible_v<E, G &&> && std::is_assignable_v<E&, G &&>)
        constexpr expected& operator=(unexpected<G>&& error) {
            if (has_value_) {
                detail::reinit(std::addressof(storage_.error), std::addressof(storage_.value),
                               std::move(error).error());
                has_value_ = false;
            } else {
                storage_.error = std::move(error).error();
            }
            return *this;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }
        constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        [[nodiscard]] constexpr T* operator->() const noexcept {
            return storage_.value;
        }
        [[nodiscard]] constexpr T& operator*() const noexcept {
            return *storage_.value;
        }
        [[nodiscard]] constexpr T& value() const {
            if (!has_value_) {
                tay::panic("bad expected access");
            }
            return *storage_.value;
        }

        [[nodiscard]] constexpr E& error() & noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr const E& error() const& noexcept {
            return storage_.error;
        }
        [[nodiscard]] constexpr E&& error() && noexcept {
            return std::move(storage_.error);
        }
        [[nodiscard]] constexpr const E&& error() const&& noexcept {
            return std::move(storage_.error);
        }

        template <typename U>
            requires std::is_convertible_v<U*, T*>
        constexpr T& emplace(U& value) noexcept {
            if (has_value_) {
                storage_.value = std::addressof(value);
            } else {
                detail::destroy_at(std::addressof(storage_.error));
                detail::construct_at(std::addressof(storage_.value), std::addressof(value));
                has_value_ = true;
            }
            return *storage_.value;
        }

        constexpr void swap(expected& other) noexcept(std::is_nothrow_move_constructible_v<E> &&
                                                      noexcept(std::swap(storage_.error,
                                                                         other.storage_.error)))
            requires(std::is_move_constructible_v<E> &&
                     requires { std::swap(storage_.error, other.storage_.error); })
        {
            if (has_value_ && other.has_value_) {
                std::swap(storage_.value, other.storage_.value);
                return;
            }
            if (!has_value_ && !other.has_value_) {
                std::swap(storage_.error, other.storage_.error);
                return;
            }
            expected* value_side = has_value_ ? this : std::addressof(other);
            expected* error_side = has_value_ ? std::addressof(other) : this;
            T* saved             = value_side->storage_.value;
            detail::destroy_at(std::addressof(value_side->storage_.value));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
            try {
                detail::construct_at(std::addressof(value_side->storage_.error),
                                     std::move(error_side->storage_.error));
            } catch (...) {
                detail::construct_at(std::addressof(value_side->storage_.value), saved);
                throw;
            }
#else
            detail::construct_at(std::addressof(value_side->storage_.error),
                                 std::move(error_side->storage_.error));
#endif
            detail::destroy_at(std::addressof(error_side->storage_.error));
            detail::construct_at(std::addressof(error_side->storage_.value), saved);
            value_side->has_value_ = false;
            error_side->has_value_ = true;
        }

        template <typename F>
        constexpr auto and_then(F&& function) & {
            return detail::and_then_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) const& {
            return detail::and_then_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) && {
            return detail::and_then_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto and_then(F&& function) const&& {
            return detail::and_then_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) & {
            return detail::transform_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) const& {
            return detail::transform_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) && {
            return detail::transform_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform(F&& function) const&& {
            return detail::transform_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) & {
            return detail::or_else_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) const& {
            return detail::or_else_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) && {
            return detail::or_else_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto or_else(F&& function) const&& {
            return detail::or_else_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) & {
            return detail::transform_error_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) const& {
            return detail::transform_error_impl(*this, std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) && {
            return detail::transform_error_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename F>
        constexpr auto transform_error(F&& function) const&& {
            return detail::transform_error_impl(std::move(*this), std::forward<F>(function));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) & {
            return detail::match_impl(*this, std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) const& {
            return detail::match_impl(*this, std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) && {
            return detail::match_impl(std::move(*this), std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) match(Visitor&& visitor) const&& {
            return detail::match_impl(std::move(*this), std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) & {
            return match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) const& {
            return match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) && {
            return std::move(*this).match(std::forward<Visitor>(visitor));
        }
        template <typename Visitor>
        constexpr decltype(auto) visit(Visitor&& visitor) const&& {
            return std::move(*this).match(std::forward<Visitor>(visitor));
        }
    };

    namespace detail {
        template <typename T>
        class ok_ref {
            T* value_;

        public:
            constexpr explicit ok_ref(T& value) noexcept : value_(std::addressof(value)) {}

            [[nodiscard]] constexpr T& get() const noexcept {
                return *value_;
            }

            template <typename V, typename E>
                requires(!std::is_void_v<V> && requires { expected<V, E>(std::declval<T&>()); })
            constexpr operator expected<V, E>() const {
                return expected<V, E>(get());
            }
        };

        template <typename T>
        struct is_ok_wrapper<ok_ref<T>> : std::true_type {};

        template <typename T>
        class ok_value {
            T value_;

        public:
            template <typename U>
                requires std::is_constructible_v<T, U&&>
            constexpr explicit ok_value(U&& value) noexcept(std::is_nothrow_constructible_v<T, U&&>)
                : value_(std::forward<U>(value)) {}

            template <typename V, typename E>
                requires(!std::is_reference_v<V> && !std::is_void_v<V> &&
                         requires { expected<V, E>(std::declval<const T&>()); })
            constexpr operator expected<V, E>() const& {
                return expected<V, E>(value_);
            }

            template <typename V, typename E>
                requires(!std::is_reference_v<V> && !std::is_void_v<V> &&
                         requires { expected<V, E>(std::declval<T&&>()); })
            constexpr operator expected<V, E>() && {
                return expected<V, E>(std::move(value_));
            }
        };

        template <typename T>
        struct is_ok_wrapper<ok_value<T>> : std::true_type {};

        struct ok_void {
            template <typename E>
            constexpr operator expected<void, E>() const {
                return expected<void, E>{};
            }
        };

        template <>
        struct is_ok_wrapper<ok_void> : std::true_type {};
    }  // namespace detail

    template <typename T>
    [[nodiscard]] constexpr auto Ok(T& value) noexcept {
        return detail::ok_ref<T>(value);
    }

    template <typename T>
        requires(!std::is_reference_v<T> &&
                 detail::valid_owned_value_type_v<std::remove_cvref_t<T>>)
    [[nodiscard]] constexpr auto Ok(T&& value) noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<T>, T&&>) {
        return detail::ok_value<std::remove_cvref_t<T>>(std::forward<T>(value));
    }

    [[nodiscard]] constexpr auto Ok() noexcept {
        return detail::ok_void{};
    }

    template <typename E>
        requires detail::valid_error_type_v<std::remove_cvref_t<E>>
    [[nodiscard]] constexpr auto Err(E&& error) noexcept(
        std::is_nothrow_constructible_v<std::remove_cvref_t<E>, E&&>) {
        return unexpected<std::remove_cvref_t<E>>(std::forward<E>(error));
    }

    template <typename T, typename E>
    constexpr void swap(expected<T, E>& left,
                        expected<T, E>& right) noexcept(noexcept(left.swap(right)))
        requires requires { left.swap(right); }
    {
        left.swap(right);
    }

    template <typename E>
    constexpr void swap(unexpected<E>& left,
                        unexpected<E>& right) noexcept(noexcept(left.swap(right)))
        requires requires { left.swap(right); }
    {
        left.swap(right);
    }

    namespace detail {
        template <typename T>
        class try_value_holder {
        public:
            constexpr explicit try_value_holder(T&& value) noexcept(
                std::is_nothrow_move_constructible_v<T>)
                : value_(std::move(value)) {}

            [[nodiscard]] constexpr T&& get() && noexcept {
                return std::move(value_);
            }

        private:
            T value_;
        };

        template <typename T>
        class try_value_holder<T&> {
        public:
            constexpr explicit try_value_holder(T& value) noexcept
                : value_(std::addressof(value)) {}

            [[nodiscard]] constexpr T& get() && noexcept {
                return *value_;
            }

        private:
            T* value_;
        };

        template <typename T>
        try_value_holder(T&&) -> try_value_holder<T>;
    }  // namespace detail
}  // namespace tay

/**
 * @brief 将 `expected` 的错误分支包装为 `tay::unexpected`。
 *
 * `result` 必须是可取错误的 `expected` 对象；错误会从结果中移动出来。
 */
#define TAY_ERR(result) tay::Err(std::move((result)).error())

/**
 * @brief 求值 `expr`，失败时从当前函数传播错误，成功时忽略其中的值。
 *
 * `expr` 只求值一次。该宏使用 Clang/GNU statement expression 扩展。
 */
#define TAY_TRYV(expr)             \
    __extension__({                \
        auto __res = (expr);       \
        if (!__res)                \
            return TAY_ERR(__res); \
    })

/**
 * @brief 求值 `expr`，失败时从当前函数传播错误，成功时提取其中的值。
 *
 * `expr` 只求值一次。值会从临时结果中移动出来；引用型 `expected` 仍返回原引用。
 */
#define TAY_TRY(expr)                                            \
    (__extension__({                                             \
        auto __res = (expr);                                     \
        if (!__res)                                              \
            return TAY_ERR(__res);                               \
        tay::detail::try_value_holder(std::move(__res).value()); \
    })).get()
