/**
 * @file refcount.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 引用计数框架
 * @version 0.1.0-dev.1
 * @date 2026-02-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace tay {
    template <typename T>
    class refc {
    protected:
        std::atomic<size_t> __refcnt;
        constexpr T *_this() {
            return static_cast<T *>(this);
        }
    public:
        constexpr refc() : __refcnt(0) {}

        [[nodiscard]]
        constexpr size_t ref_count() const {
            return __refcnt;
        }

        [[nodiscard]]
        constexpr bool alive() const {
            return __refcnt > 0;
        }

        constexpr void keep() {
            __refcnt++;
        }

        constexpr void release() {
            if (!alive()) {
                return;
            }
            __refcnt--;
            if (!alive()) {
                _this()->on_death();
            }
        }
    };

    template <typename T>
    class refc_ptr {
    private:
        T *_ptr;
    public:
        constexpr refc_ptr(T *ptr) : _ptr(ptr) {
            if (_ptr)
                _ptr->keep();
        }
        constexpr refc_ptr(const refc_ptr<T> &other) : _ptr(other._ptr) {
            if (_ptr)
                _ptr->keep();
        }
        constexpr refc_ptr(refc_ptr<T> &&other) : _ptr(other._ptr) {
            if (_ptr)
                _ptr->keep();
        }
        constexpr refc_ptr<T> &operator=(const refc_ptr<T> &other) {
            if (_ptr == other._ptr) {
                return *this;
            }
            if (_ptr) {
                _ptr->release();
            }
            _ptr = other._ptr;
            if (_ptr) {
                _ptr->keep();
            }
            return *this;
        }
        constexpr refc_ptr<T> &operator=(refc_ptr<T> &&other) {
            if (_ptr == other._ptr) {
                return *this;
            }
            if (_ptr) {
                _ptr->release();
            }
            _ptr = other._ptr;
            if (_ptr) {
                _ptr->keep();
            }
            return *this;
        }
        constexpr ~refc_ptr() {
            if (_ptr)
                _ptr->release();
        }
        constexpr T *get() const {
            return _ptr;
        }
        constexpr T *operator->() const {
            return _ptr;
        }
        constexpr T &operator*() const {
            return *_ptr;
        }
        constexpr operator T*() const {
            return _ptr;
        }
    };
}  // namespace util
