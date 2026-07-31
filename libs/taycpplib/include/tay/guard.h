/**
 * @file guard.h
 * @brief Scope-bound cleanup guard.
 * @version 0.1.0-dev.1
 * @date 2026-07-31
 */

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace tay {
    /**
     * @brief Invoke a callable when the current scope is left.
     *
     * A guard is deliberately neither copyable nor movable: its lifetime is
     * the scope in which it is created. Call release() to disarm it after the
     * guarded operation has been committed successfully.
     */
    template <typename Fn>
        requires std::invocable<Fn&>
    class guard {
    public:
        constexpr explicit guard(Fn fn) noexcept(
            std::is_nothrow_move_constructible_v<Fn>)
            : fn_(std::move(fn)), active_(true) {}

        guard(const guard&)            = delete;
        guard& operator=(const guard&) = delete;
        guard(guard&&)                 = delete;
        guard& operator=(guard&&)      = delete;

        constexpr ~guard() noexcept {
            if (active_) {
                fn_();
            }
        }

        constexpr void release() noexcept {
            active_ = false;
        }

        [[nodiscard]]
        constexpr bool active() const noexcept {
            return active_;
        }

    private:
        [[no_unique_address]] Fn fn_;
        bool active_;
    };

    template <typename Fn>
    guard(Fn) -> guard<std::decay_t<Fn>>;
}  // namespace tay
