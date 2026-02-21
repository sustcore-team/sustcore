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
    struct DeferEntry {
        using ctor_type = void (*)(void *);
        void *_instance;
        ctor_type _constructor;
    };

    consteval DeferEntry __make_defer_entry(void *instance,
                                            DeferEntry::ctor_type constructor) {
        return DeferEntry{instance, constructor};
    }

    template <typename T, auto... args>
    class Defer {
    protected:
        alignas(T) char storage[sizeof(T)];
        bool initialized = false;
        inline static void static_construct(void *defer) {
            assert(defer != nullptr);
            Defer<T> *_defer = static_cast<Defer<T> *>(defer);
            _defer->construct();
        }
        inline T*_get(void) {
            return std::launder(reinterpret_cast<T *>(storage));
        }
    public:
        inline void construct() {
            assert(!initialized);
            new (storage) T(args...);
            initialized = true;
        }
        inline T &get() {
            assert(initialized);
            return *_get();
        }
        inline T *operator->() {
            return &get();
        }
        inline T &operator*() {
            return get();
        }
        operator T&() {
            return get();
        }

        consteval DeferEntry make_defer(void) {
            return __make_defer_entry((void *)this, &Defer<T>::static_construct);
        }
    };

#define DEFER_SECTION(phase) __attribute__((section(".defer."#phase), used))
#define PRE_DEFER DEFER_SECTION(pre)
#define POST_DEFER DEFER_SECTION(post)
#define AutoDefer(x, phase)                                                \
    inline __attribute__((section(".defer."#phase), used)) util::DeferEntry \
        __auto_defer_##x = x.make_defer()
#define AutoDeferPre(x)  AutoDefer(x, pre)
#define AutoDeferPost(x) AutoDefer(x, post)
}  // namespace util