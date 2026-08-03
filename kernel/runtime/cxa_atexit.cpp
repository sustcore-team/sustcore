/**
 * @file cxa_atexit.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief C++ 静态对象析构 ABI 支持
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

// Itanium C++ ABI: DSO 析构函数登记与逆序调用。
#include <runtime/cxa_dso.h>
#include <tay/lock.h>
#include <tay/spinlock.h>

#include <cstddef>

namespace __cxa {
    constexpr size_t __DESTRUCTOR_CAPS = 256;
    struct DestructorRecord {
        void (*function)(void *) = nullptr;
        void *argument           = nullptr;
        void *dso                = nullptr;
        bool active              = false;
    };

    constinit DestructorRecord __destructors[__DESTRUCTOR_CAPS];
    constinit tay::ticket_spinlock __destructor_lock;

    int __cxa_atexit(void (*function)(void *), void *argument, void *dso) {
        tay::lock_guard guard(__destructor_lock);
        for (auto &record : __destructors) {
            if (!record.active) {
                record = DestructorRecord{
                    .function = function, .argument = argument, .dso = dso, .active = true};
                return 0;
            }
        }
        return -1;
    }

    int __cxa_finalize(void *dso) {
        while (true) {
            void (*function)(void *) = nullptr;
            void *argument           = nullptr;
            {
                tay::lock_guard guard(__destructor_lock);
                // 在锁内先将记录标为失效，再在锁外逆序调用，允许析构函数再次登记或 finalize。
                for (size_t idx = __DESTRUCTOR_CAPS; idx > 0; --idx) {
                    auto &record = __destructors[idx - 1];
                    if (record.active && (dso == nullptr || record.dso == dso)) {
                        record.active = false;
                        function      = record.function;
                        argument      = record.argument;
                        break;
                    }
                }
            }
            if (function == nullptr) {
                return 0;
            }
            function(argument);
        }
    }
}  // namespace __cxa

extern "C" {
int __cxa_atexit(void (*function)(void *), void *argument, void *dso) {
    return __cxa::__cxa_atexit(function, argument, dso);
}

void __cxa_finalize(void *dso) {
    __cxa::__cxa_finalize(dso);
}
}  // extern "C"