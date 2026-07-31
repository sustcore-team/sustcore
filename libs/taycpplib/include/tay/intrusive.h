/**
 * @file intrusive.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief intrusive traits
 * @version 0.1.0-dev.1
 * @date 2026-07-30
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

namespace tay {
    template <typename T, typename OwnerPointer, typename BorrowPointer>
    struct intrusive_traits;

    template <typename T>
    struct intrusive_traits<T, T *, T *> {
        using owner_pointer = T *;
        using borrow_pointer = T *;
        using const_borrow_pointer = const T *;

        static constexpr T *decay(T *owner) noexcept {
            return owner;
        }
    };

    template <typename T, typename H, H T:: *Member>
    struct locate_member {
        constexpr H &operator()(T &x) const noexcept {
            return x.*Member;
        }
        constexpr const H &operator()(const T &x) const noexcept {
            return x.*Member;
        }
    };
}
