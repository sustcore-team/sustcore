/**
 * @file nonnull.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 非空指针类型
 * @version alpha-1.0.0
 * @date 2026-04-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>
#include <cassert>
#include <type_traits>
#include <nt/errors.h>

namespace util
{
    template <typename T, bool enabled = std::is_pointer_v<T>>
    class nonnull;

    template <typename T>
    class nonnull<T*, true> {
    private:
        T *ptr;
        constexpr explicit nonnull(T *p) : ptr(p) {
            assert (p != nullptr);
        }
    public:
        constexpr nonnull(std::nullptr_t) = delete;
        static constexpr std::result_nt<nonnull<T*, true>> from(T *p)
        {
            if (p == nullptr) {
                return std::unexpected(std::error_type::NULLPTR);
            }
            return nonnull<T*, true>(p);
        }

        static constexpr std::result_nt<nonnull<T*, true>> from(std::nullptr_t) = delete;

        T *get() const {
            return ptr;
        }

        T &operator*() const {
            return *ptr;
        }

        T *operator->() const {
            return ptr;
        }

        operator T *() const {
            return ptr;
        }
    };

    template <typename T>
    constexpr std::result_nt<nonnull<T*, true>> nonnull_from(T *p)
    {
        return nonnull<T *, true>::from(p);
    }

    // Create nonnull from reference to avoid the result wrapper
    template <typename T>
    constexpr nonnull<T*, true> nonnull_from(T &p)
    {
        return nonnull<T *, true>::from(&p).value();
    }

    template <typename T>
    constexpr nonnull<T*, true> guarantee_nonnull(T *p)
    {
        assert (p != nullptr);
        return nonnull<T *, true>::from(p).value();
    }
}