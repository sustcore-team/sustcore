/**
 * @file optional.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供无异常、固定存储的可选值类型。
 * @version 0.1.0-dev.1
 * @date 2026-08-18
 *
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <tay/in_place.h>
#include <tay/panic.h>

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace tay {
    struct nullopt_t {
        explicit constexpr nullopt_t() = default;
    };

    inline constexpr nullopt_t nullopt{};

    template <typename T>
    class optional;

    namespace detail {
        template <typename T>
        struct is_optional : std::false_type {};

        template <typename T>
        struct is_optional<optional<T>> : std::true_type {};

        template <typename T>
        inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

        template <typename T>
        union optional_storage {
            char empty;
            T value;

            constexpr optional_storage() noexcept : empty{} {}
            constexpr ~optional_storage() {}
        };
    }  // namespace detail

    /**
     * @brief 保存零个或一个 `T`，不分配内存且不使用异常。
     *
     * 对空对象调用 `value()` 会触发 `tay::panic()`；不检查状态的 `operator*` 和
     * `operator->` 要求调用方已经确认对象非空。
     */
    template <typename T>
    class optional {
        static_assert(std::is_object_v<T> && !std::is_array_v<T>,
                      "optional<T> requires a non-array object");
        static_assert(std::is_same_v<T, std::remove_cv_t<T>>,
                      "optional<T> requires an unqualified value type");

    public:
        using value_type = T;

        constexpr optional() noexcept = default;
        constexpr optional(nullopt_t) noexcept {}

        template <typename U = T>
            requires(!detail::is_optional_v<U> &&
                     !std::same_as<std::remove_cvref_t<U>, nullopt_t> &&
                     !std::same_as<std::remove_cvref_t<U>, in_place_t> &&
                     std::constructible_from<T, U &&>)
        constexpr explicit(!std::convertible_to<U &&, T>)
            optional(U &&value) noexcept(std::is_nothrow_constructible_v<T, U &&>) {
            construct(std::forward<U>(value));
        }

        template <typename... Args>
            requires std::constructible_from<T, Args &&...>
        constexpr explicit optional(in_place_t, Args &&...args) noexcept(
            std::is_nothrow_constructible_v<T, Args &&...>) {
            construct(std::forward<Args>(args)...);
        }

        constexpr optional(const optional &other) noexcept(std::is_nothrow_copy_constructible_v<T>)
            requires std::copy_constructible<T>
        {
            if (other.has_value_)
                construct(other.storage_.value);
        }

        constexpr optional(const optional &)
            requires(!std::copy_constructible<T>)
        = delete;

        constexpr optional(optional &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
            requires std::move_constructible<T>
        {
            if (other.has_value_)
                construct(std::move(other.storage_.value));
        }

        constexpr optional(optional &&)
            requires(!std::move_constructible<T>)
        = delete;

        constexpr ~optional() noexcept {
            reset();
        }

        constexpr optional &operator=(nullopt_t) noexcept {
            reset();
            return *this;
        }

        constexpr optional &operator=(const optional &other) noexcept(
            std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_assignable_v<T>)
            requires(std::copy_constructible<T> && std::is_copy_assignable_v<T>)
        {
            if (this == &other)
                return *this;
            assign_from(other);
            return *this;
        }

        constexpr optional &operator=(const optional &)
            requires(!(std::copy_constructible<T> && std::is_copy_assignable_v<T>))
        = delete;

        constexpr optional &operator=(optional &&other) noexcept(
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
            requires(std::move_constructible<T> && std::is_move_assignable_v<T>)
        {
            if (this == &other)
                return *this;
            assign_from(std::move(other));
            return *this;
        }

        constexpr optional &operator=(optional &&)
            requires(!(std::move_constructible<T> && std::is_move_assignable_v<T>))
        = delete;

        template <typename U = T>
            requires(!detail::is_optional_v<U> && std::constructible_from<T, U &&> &&
                     std::assignable_from<T &, U &&>)
        constexpr optional &operator=(U &&value) noexcept(
            std::is_nothrow_constructible_v<T, U &&> && std::is_nothrow_assignable_v<T &, U &&>) {
            if (has_value_)
                storage_.value = std::forward<U>(value);
            else
                construct(std::forward<U>(value));
            return *this;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return has_value_;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return has_value_;
        }

        [[nodiscard]] constexpr T *operator->() noexcept {
            return std::addressof(storage_.value);
        }

        [[nodiscard]] constexpr const T *operator->() const noexcept {
            return std::addressof(storage_.value);
        }

        [[nodiscard]] constexpr T &operator*() & noexcept {
            return storage_.value;
        }

        [[nodiscard]] constexpr const T &operator*() const & noexcept {
            return storage_.value;
        }

        [[nodiscard]] constexpr T &&operator*() && noexcept {
            return std::move(storage_.value);
        }

        [[nodiscard]] constexpr const T &&operator*() const && noexcept {
            return std::move(storage_.value);
        }

        [[nodiscard]] constexpr T &value() & noexcept {
            require_value();
            return storage_.value;
        }

        [[nodiscard]] constexpr const T &value() const & noexcept {
            require_value();
            return storage_.value;
        }

        [[nodiscard]] constexpr T &&value() && noexcept {
            require_value();
            return std::move(storage_.value);
        }

        [[nodiscard]] constexpr const T &&value() const && noexcept {
            require_value();
            return std::move(storage_.value);
        }

        template <typename U>
            requires(std::copy_constructible<T> && std::convertible_to<U &&, T>)
        [[nodiscard]] constexpr T value_or(U &&default_value) const & noexcept(
            std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_constructible_v<T, U &&>) {
            if (has_value_)
                return storage_.value;
            return static_cast<T>(std::forward<U>(default_value));
        }

        template <typename U>
            requires(std::move_constructible<T> && std::convertible_to<U &&, T>)
        [[nodiscard]] constexpr T value_or(U &&default_value) && noexcept(
            std::is_nothrow_move_constructible_v<T> && std::is_nothrow_constructible_v<T, U &&>) {
            if (has_value_)
                return std::move(storage_.value);
            return static_cast<T>(std::forward<U>(default_value));
        }

        constexpr void reset() noexcept {
            if (!has_value_)
                return;
            std::destroy_at(std::addressof(storage_.value));
            has_value_ = false;
        }

        template <typename... Args>
            requires std::constructible_from<T, Args &&...>
        constexpr T &emplace(Args &&...args) noexcept(
            std::is_nothrow_constructible_v<T, Args &&...>) {
            reset();
            return construct(std::forward<Args>(args)...);
        }

    private:
        template <typename... Args>
        constexpr T &construct(Args &&...args) noexcept(
            std::is_nothrow_constructible_v<T, Args &&...>) {
            auto *value =
                std::construct_at(std::addressof(storage_.value), std::forward<Args>(args)...);
            has_value_ = true;
            return *value;
        }

        template <typename Other>
        constexpr void assign_from(Other &&other) noexcept(
            std::is_nothrow_constructible_v<T, decltype(*std::forward<Other>(other))> &&
            std::is_nothrow_assignable_v<T &, decltype(*std::forward<Other>(other))>) {
            if (has_value_ && other.has_value_)
                storage_.value = *std::forward<Other>(other);
            else if (other.has_value_)
                construct(*std::forward<Other>(other));
            else
                reset();
        }

        constexpr void require_value() const noexcept {
            if (!has_value_)
                tay::panic("bad optional access");
        }

        detail::optional_storage<T> storage_{};
        bool has_value_ = false;
    };

    template <typename T>
    optional(T) -> optional<T>;

    template <typename T, typename U>
    [[nodiscard]] constexpr bool operator==(const optional<T> &left,
                                            const optional<U> &right) noexcept(noexcept(*left ==
                                                                                        *right)) {
        if (left.has_value() != right.has_value())
            return false;
        return !left.has_value() || *left == *right;
    }

    template <typename T>
    [[nodiscard]] constexpr bool operator==(const optional<T> &value, nullopt_t) noexcept {
        return !value.has_value();
    }

    template <typename T>
    [[nodiscard]] constexpr bool operator==(nullopt_t, const optional<T> &value) noexcept {
        return !value.has_value();
    }
}  // namespace tay
