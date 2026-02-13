/**
 * @file defer.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 延迟构造
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * 由于许多全局对象并非从一开始就能被构造,
 * 我们引入defer机制, 在条件成熟时统一构造这些对象
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/list.h>

#include <cassert>
#include <new>

namespace util {
    template <typename T, auto... Args>
    class Defer {
    protected:
        alignas(T) char storage[sizeof(T)];
        bool initialized = false;
    public:
        inline void construct() {
            assert(! initialized);
            new (storage) T(Args...);
            initialized = true;
        }
        inline T &get() {
            assert(initialized);
            return *reinterpret_cast<T *>(storage);
        }
    };
}  // namespace util