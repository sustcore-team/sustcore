/**
 * @file unique_ptr.h
 * @brief Exclusive-ownership smart pointers for objects and arrays.
 * @version 0.1.0-dev.1
 * @date 2026-07-31
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include <tay/owner.h>

namespace tay {
    template <typename T>
    class unique_ptr;

    /**
     * @brief Exclusive ownership of one dynamically allocated object.
     */
    template <typename T>
    class unique_ptr {
        static_assert(!std::is_array_v<T>,
                      "use tay::unique_ptr<T[]> for arrays");
        static_assert(!std::is_void_v<T>,
                      "tay::unique_ptr<void> is not supported");

        template <typename U>
        friend class unique_ptr;

    public:
        using pointer      = T*;
        using element_type = T;

        constexpr unique_ptr() noexcept = default;
        constexpr unique_ptr(std::nullptr_t) noexcept {}

        constexpr explicit unique_ptr(pointer ptr) noexcept : ptr_(ptr) {}

        template <typename U>
            requires std::is_convertible_v<U*, pointer>
        constexpr explicit unique_ptr(owner<U*>&& ptr) noexcept
            : ptr_(static_cast<pointer>(ptr.get())) {
            ptr = owner<U*>{nullptr};
        }

        unique_ptr(const unique_ptr&)            = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        constexpr unique_ptr(unique_ptr&& other) noexcept
            : ptr_(other.release()) {}

        template <typename U>
            requires(!std::is_array_v<U> &&
                     std::is_convertible_v<U*, pointer>)
        constexpr unique_ptr(unique_ptr<U>&& other) noexcept
            : ptr_(other.release()) {}

        constexpr ~unique_ptr() {
            delete ptr_;
        }

        constexpr unique_ptr& operator=(unique_ptr&& other) noexcept {
            if (this != &other) {
                reset(other.release());
            }
            return *this;
        }

        template <typename U>
            requires(!std::is_array_v<U> &&
                     std::is_convertible_v<U*, pointer>)
        constexpr unique_ptr& operator=(unique_ptr<U>&& other) noexcept {
            reset(other.release());
            return *this;
        }

        constexpr unique_ptr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        [[nodiscard]]
        constexpr pointer get() const noexcept {
            return ptr_;
        }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept {
            return ptr_ != nullptr;
        }

        [[nodiscard]]
        constexpr element_type& operator*() const noexcept {
            return *ptr_;
        }

        [[nodiscard]]
        constexpr pointer operator->() const noexcept {
            return ptr_;
        }

        [[nodiscard]]
        constexpr pointer release() noexcept {
            pointer old = ptr_;
            ptr_        = nullptr;
            return old;
        }

        [[nodiscard]]
        constexpr owner<pointer> release_owner() noexcept {
            return owner<pointer>{release()};
        }

        constexpr void reset(pointer ptr = nullptr) noexcept {
            if (ptr_ == ptr) {
                return;
            }
            pointer old = ptr_;
            ptr_        = ptr;
            delete old;
        }

        constexpr void swap(unique_ptr& other) noexcept {
            using std::swap;
            swap(ptr_, other.ptr_);
        }

    private:
        pointer ptr_ = nullptr;
    };

    /**
     * @brief Exclusive ownership of a dynamically allocated array.
     */
    template <typename T>
    class unique_ptr<T[]> {
        template <typename U>
        friend class unique_ptr;

    public:
        using pointer      = T*;
        using element_type = T;

        constexpr unique_ptr() noexcept = default;
        constexpr unique_ptr(std::nullptr_t) noexcept {}

        template <typename U>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr explicit unique_ptr(U* ptr) noexcept : ptr_(ptr) {}

        template <typename U>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr explicit unique_ptr(owner<U*>&& ptr) noexcept
            : ptr_(ptr.get()) {
            ptr = owner<U*>{nullptr};
        }

        unique_ptr(const unique_ptr&)            = delete;
        unique_ptr& operator=(const unique_ptr&) = delete;

        constexpr unique_ptr(unique_ptr&& other) noexcept
            : ptr_(other.release()) {}

        template <typename U>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr unique_ptr(unique_ptr<U[]>&& other) noexcept
            : ptr_(other.release()) {}

        constexpr ~unique_ptr() {
            delete[] ptr_;
        }

        constexpr unique_ptr& operator=(unique_ptr&& other) noexcept {
            if (this != &other) {
                reset(other.release());
            }
            return *this;
        }

        template <typename U>
            requires std::is_convertible_v<U (*)[], T (*)[]>
        constexpr unique_ptr& operator=(unique_ptr<U[]>&& other) noexcept {
            reset(other.release());
            return *this;
        }

        constexpr unique_ptr& operator=(std::nullptr_t) noexcept {
            reset();
            return *this;
        }

        [[nodiscard]]
        constexpr pointer get() const noexcept {
            return ptr_;
        }

        [[nodiscard]]
        constexpr explicit operator bool() const noexcept {
            return ptr_ != nullptr;
        }

        [[nodiscard]]
        constexpr element_type& operator[](std::size_t index) const noexcept {
            return ptr_[index];
        }

        [[nodiscard]]
        constexpr pointer release() noexcept {
            pointer old = ptr_;
            ptr_        = nullptr;
            return old;
        }

        [[nodiscard]]
        constexpr owner<pointer> release_owner() noexcept {
            return owner<pointer>{release()};
        }

        constexpr void reset(pointer ptr = nullptr) noexcept {
            if (ptr_ == ptr) {
                return;
            }
            pointer old = ptr_;
            ptr_        = ptr;
            delete[] old;
        }

        constexpr void swap(unique_ptr& other) noexcept {
            using std::swap;
            swap(ptr_, other.ptr_);
        }

    private:
        pointer ptr_ = nullptr;
    };

    template <typename T>
    unique_ptr(T*) -> unique_ptr<T>;

    template <typename T>
    constexpr void swap(unique_ptr<T>& lhs, unique_ptr<T>& rhs) noexcept {
        lhs.swap(rhs);
    }

    template <typename T, typename... Args>
        requires(!std::is_array_v<T>)
    [[nodiscard]]
    unique_ptr<T> make_unique(Args&&... args) {
        return unique_ptr<T>{new T(std::forward<Args>(args)...)};
    }

    template <typename T>
        requires(std::is_array_v<T> && std::extent_v<T> == 0)
    [[nodiscard]]
    unique_ptr<T> make_unique(std::size_t count) {
        using element_type = typename unique_ptr<T>::element_type;
        return unique_ptr<T>{new element_type[count]()};
    }

    template <typename T, typename... Args>
        requires(std::is_array_v<T> && std::extent_v<T> != 0)
    void make_unique(Args&&...) = delete;
}  // namespace tay
