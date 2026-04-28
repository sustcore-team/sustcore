/**
 * @file raii.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief raii
 * @version alpha-1.0.0
 * @date 2026-02-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cassert>
#include <concepts>
#include <cstddef>

namespace util {

    // call the deleter automatically to ensure
    // the resource is correctly released when the guard goes out of scope
    template <typename Deleter>
    class Guard {
    private:
        Deleter deleter;
        bool released = false;

    public:
        Guard(Deleter deleter) : deleter(deleter) {}
        ~Guard() {
            if (!released)
                deleter();
        }
        void release() noexcept {
            released = true;
        }
    };

    // deduction guide for Guard
    template <typename T>
    Guard(T) -> Guard<T>;
}  // namespace util