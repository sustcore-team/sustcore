/**
 * @file refcount.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 引用计数框架
 * @version alpha-1.0.0
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>
#include <type_traits>

namespace util {
    template <typename T>
    class RefCount {
    protected:
        size_t _ref_count;
        inline T *_this(void) {
            return static_cast<T *>(this);
        }
    public:
        RefCount() : _ref_count(0) {}

        constexpr size_t ref_count(void) const {
            return _ref_count;
        }

        constexpr bool alive(void) const {
            return _ref_count > 0;
        }

        inline void retain(void) {
            _ref_count++;
        }

        inline void release(void) {
            if (!alive()) {
                return;
            }
            _ref_count--;
            if (!alive()) {
                _this()->on_death();
            }
        }
    };
}  // namespace util