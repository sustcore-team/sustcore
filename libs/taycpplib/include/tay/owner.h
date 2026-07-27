/**
 * @file owner.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief owner 指针标注, 参考 gsl::owner
 * @version 0.1.0-dev.1
 * @date 2026-03-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <type_traits>

namespace tay {
    /**
     * @brief 表示拥有所有权的指针类型
     *
     * @tparam T 指针指向的类型
     */
    template <typename T, bool enabled = std::is_pointer_v<T>>
    class owner;

    /**
     * @brief 表示拥有所有权的指针类型
     *
     * 通过删除 operator=(pointer) 来强制保证裸指针不能直接转换为 owner 类型
     *
     * @tparam T 指针指向的类型
     */
    template <typename T>
    class owner<T*, true> {
        using value_type = T;
        using pointer    = value_type*;
        using reference  = value_type&;

    private:
        pointer ptr;

    public:
        constexpr explicit owner(pointer p = nullptr) : ptr(p) {}

        template <typename U, typename = std::enable_if_t<
                                  std::is_convertible_v<U*, pointer>>>
        constexpr owner(owner<U*> other) : ptr(other.get()) {}

        ~owner() = default;

        constexpr owner(const owner&)             = default;
        constexpr owner& operator=(const owner&)  = default;
        constexpr owner(owner&& other)            = default;
        constexpr owner& operator=(owner&& other) = default;
        // 禁止直接从裸指针转换为 owner
        constexpr owner& operator=(pointer)       = delete;

        pointer get() const {
            return ptr;
        }

        reference operator*() const {
            return *ptr;
        }

        pointer operator->() const {
            return ptr;
        }

        explicit operator bool() const {
            return ptr != nullptr;
        }

        operator pointer() const {
            return ptr;
        }
    };

    template <typename T>
    owner(T*) -> owner<T*>;
}  // namespace tay
