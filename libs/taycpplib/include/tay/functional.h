/**
 * @file functional.h
 * @brief Non-owning and fixed-storage callable wrappers.
 */

#pragma once

#include <tay/panic.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace tay {
    template <class Signature>
    class function_ref;

    template <class R, class... Args>
    class function_ref<R(Args...)> {
        using callback_type = R (*)(void*, Args&&...);
        void* object_ = nullptr;
        callback_type callback_ = nullptr;
        R (*function_)(Args...) = nullptr;

    public:
        function_ref() = delete;

        template <class F>
            requires(!std::same_as<std::remove_cvref_t<F>, function_ref> &&
                     std::is_object_v<F> &&
                     std::is_invocable_r_v<R, F&, Args...>)
        constexpr function_ref(F& callable) noexcept
            : object_(const_cast<void*>(static_cast<const void*>(
                  std::addressof(callable)))),
              callback_([](void* object, Args&&... args) -> R {
                  if constexpr (std::is_void_v<R>) {
                      std::invoke(*static_cast<F*>(object),
                                  std::forward<Args>(args)...);
                  } else {
                      return std::invoke(*static_cast<F*>(object),
                                         std::forward<Args>(args)...);
                  }
              }) {}

        constexpr function_ref(R (&function)(Args...)) noexcept
            : function_(&function) {}

        [[nodiscard]] constexpr R operator()(Args... args) const {
            if constexpr (std::is_void_v<R>) {
                if (function_ != nullptr)
                    function_(std::forward<Args>(args)...);
                else
                    callback_(object_, std::forward<Args>(args)...);
            } else {
                return function_ != nullptr
                           ? function_(std::forward<Args>(args)...)
                           : callback_(object_, std::forward<Args>(args)...);
            }
        }
    };

    template <class R, class... Args>
    class function_ref<R(Args...) noexcept> {
        using callback_type = R (*)(void*, Args&&...) noexcept;
        void* object_ = nullptr;
        callback_type callback_ = nullptr;
        R (*function_)(Args...) noexcept = nullptr;

    public:
        function_ref() = delete;

        template <class F>
            requires(!std::same_as<std::remove_cvref_t<F>, function_ref> &&
                     std::is_object_v<F> &&
                     std::is_nothrow_invocable_r_v<R, F&, Args...>)
        constexpr function_ref(F& callable) noexcept
            : object_(const_cast<void*>(static_cast<const void*>(
                  std::addressof(callable)))),
              callback_([](void* object, Args&&... args) noexcept -> R {
                  if constexpr (std::is_void_v<R>) {
                      std::invoke(*static_cast<F*>(object),
                                  std::forward<Args>(args)...);
                  } else {
                      return std::invoke(*static_cast<F*>(object),
                                         std::forward<Args>(args)...);
                  }
              }) {}

        constexpr function_ref(R (&function)(Args...) noexcept) noexcept
            : function_(&function) {}

        [[nodiscard]] constexpr R operator()(Args... args) const noexcept {
            if constexpr (std::is_void_v<R>) {
                if (function_ != nullptr)
                    function_(std::forward<Args>(args)...);
                else
                    callback_(object_, std::forward<Args>(args)...);
            } else {
                return function_ != nullptr
                           ? function_(std::forward<Args>(args)...)
                           : callback_(object_, std::forward<Args>(args)...);
            }
        }
    };

    template <class Signature, std::size_t N>
    class inplace_function;

    namespace detail {
        template <class R, bool Noexcept, class... Args>
        struct inplace_function_vtable {
            using invoke_type = std::conditional_t<
                Noexcept, R (*)(void*, Args&&...) noexcept,
                R (*)(void*, Args&&...)>;
            invoke_type invoke;
            void (*copy)(const void*, void*) noexcept;
            void (*move)(void*, void*) noexcept;
            void (*destroy)(void*) noexcept;
        };
    }  // namespace detail

    template <class R, class... Args, std::size_t N>
    class inplace_function<R(Args...), N> {
        using vtable_type =
            detail::inplace_function_vtable<R, false, Args...>;
        alignas(std::max_align_t) unsigned char storage_[N == 0 ? 1 : N];
        const vtable_type* vtable_ = nullptr;

        template <class F>
        static constexpr vtable_type table_for{
            [](void* object, Args&&... args) -> R {
                if constexpr (std::is_void_v<R>) {
                    std::invoke(*static_cast<F*>(object),
                                std::forward<Args>(args)...);
                } else {
                    return std::invoke(*static_cast<F*>(object),
                                       std::forward<Args>(args)...);
                }
            },
            [](const void* source, void* target) noexcept {
                static_cast<void>(std::construct_at(
                    static_cast<F*>(target), *static_cast<const F*>(source)));
            },
            [](void* source, void* target) noexcept {
                static_cast<void>(std::construct_at(
                    static_cast<F*>(target),
                    std::move(*static_cast<F*>(source))));
                std::destroy_at(static_cast<F*>(source));
            },
            [](void* object) noexcept { std::destroy_at(static_cast<F*>(object)); }};

    public:
        constexpr inplace_function() noexcept = default;
        constexpr inplace_function(std::nullptr_t) noexcept {}

        template <class F>
            requires(!std::same_as<std::remove_cvref_t<F>, inplace_function> &&
                     std::is_invocable_r_v<R, std::decay_t<F>&, Args...> &&
                     std::is_nothrow_copy_constructible_v<std::decay_t<F>> &&
                     std::is_nothrow_move_constructible_v<std::decay_t<F>> &&
                     std::is_nothrow_destructible_v<std::decay_t<F>> &&
                     sizeof(std::decay_t<F>) <= N &&
                     alignof(std::decay_t<F>) <= alignof(std::max_align_t))
        constexpr inplace_function(F&& callable) noexcept {
            using callable_type = std::decay_t<F>;
            static_cast<void>(std::construct_at(
                reinterpret_cast<callable_type*>(storage_),
                std::forward<F>(callable)));
            vtable_ = &table_for<callable_type>;
        }

        constexpr inplace_function(const inplace_function& other) noexcept {
            if (other.vtable_ != nullptr) {
                other.vtable_->copy(other.storage_, storage_);
                vtable_ = other.vtable_;
            }
        }
        constexpr inplace_function(inplace_function&& other) noexcept {
            if (other.vtable_ != nullptr) {
                other.vtable_->move(other.storage_, storage_);
                vtable_ = other.vtable_;
                other.vtable_ = nullptr;
            }
        }
        constexpr inplace_function& operator=(
            const inplace_function& other) noexcept {
            if (this != &other) {
                reset();
                if (other.vtable_ != nullptr) {
                    other.vtable_->copy(other.storage_, storage_);
                    vtable_ = other.vtable_;
                }
            }
            return *this;
        }
        constexpr inplace_function& operator=(
            inplace_function&& other) noexcept {
            if (this != &other) {
                reset();
                if (other.vtable_ != nullptr) {
                    other.vtable_->move(other.storage_, storage_);
                    vtable_ = other.vtable_;
                    other.vtable_ = nullptr;
                }
            }
            return *this;
        }
        constexpr ~inplace_function() noexcept { reset(); }

        constexpr void reset() noexcept {
            if (vtable_ != nullptr) {
                vtable_->destroy(storage_);
                vtable_ = nullptr;
            }
        }
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return vtable_ != nullptr;
        }
        constexpr R operator()(Args... args) const {
            if (vtable_ == nullptr) {
                tay::panic("empty inplace_function invocation");
            }
            if constexpr (std::is_void_v<R>) {
                vtable_->invoke(const_cast<unsigned char*>(storage_),
                                std::forward<Args>(args)...);
            } else {
                return vtable_->invoke(const_cast<unsigned char*>(storage_),
                                       std::forward<Args>(args)...);
            }
        }
    };

    template <class R, class... Args, std::size_t N>
    class inplace_function<R(Args...) noexcept, N> {
        using inner_type = inplace_function<R(Args...), N>;
        inner_type inner_;

    public:
        constexpr inplace_function() noexcept = default;
        constexpr inplace_function(std::nullptr_t) noexcept {}
        template <class F>
            requires std::is_nothrow_invocable_r_v<R, std::decay_t<F>&,
                                                   Args...>
        constexpr inplace_function(F&& callable) noexcept
            : inner_(std::forward<F>(callable)) {}
        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(inner_);
        }
        constexpr void reset() noexcept { inner_.reset(); }
        constexpr R operator()(Args... args) const noexcept {
            if constexpr (std::is_void_v<R>) {
                inner_(std::forward<Args>(args)...);
            } else {
                return inner_(std::forward<Args>(args)...);
            }
        }
    };
}  // namespace tay
