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

#include <nt/errors.h>

#include <cassert>
#include <cstddef>
#include <type_traits>

namespace util {
    template <typename T, bool enabled = std::is_pointer_v<T>>
    class nonnull;

    template <typename>
    struct is_nonnull : std::false_type {};

    template <typename T, bool enabled>
    struct is_nonnull<nonnull<T, enabled>> : std::true_type {};

    template <typename T>
    inline constexpr bool is_nonnull_v = is_nonnull<T>::value;

    template <typename T>
    class nonnull<T *, true> {
    private:
        T *ptr;
        constexpr explicit nonnull(T *p) : ptr(p) {
            assert(p != nullptr);
        }

    public:
        constexpr nonnull(std::nullptr_t) = delete;

        constexpr nonnull(const nonnull<T *, true> &other) : ptr(other.ptr) {}
        constexpr nonnull(nonnull<T *, true> &&other) : ptr(other.ptr) {}

        constexpr nonnull<T *, true> &operator=(const nonnull<T *, true> &other) {
            ptr = other.ptr;
            return *this;
        }

        constexpr nonnull<T *, true> &operator=(nonnull<T *, true> &&other) {
            ptr = other.ptr;
            return *this;
        }

        constexpr ~nonnull() = default;


        template <typename U>
        constexpr nonnull(nonnull<U *, true> other)
            : ptr(static_cast<T *>(other.get())) {
            static_assert(std::is_convertible_v<U *, T *>,
                          "Incompatible pointer type");
            assert(ptr != nullptr);
        }

        template <typename U>
            requires(std::is_convertible_v<U *, T *> &&
                     !is_nonnull_v<std::remove_cvref_t<U>>)
        constexpr nonnull(U &ref) : nonnull(static_cast<T *>(&ref)) {}

        constexpr nonnull<T *, true> operator=(std::nullptr_t) = delete;
        template <typename U>
        constexpr nonnull<T *, true>& operator=(nonnull<U *, true> other) {
            static_assert(std::is_convertible_v<U *, T *>,
                          "Incompatible pointer type");
            ptr = static_cast<T *>(other.get());
            assert(ptr != nullptr);
            return *this;
        }

        template <typename U>
            requires(std::is_convertible_v<U *, T *> &&
                     !is_nonnull_v<std::remove_cvref_t<U>>)
        constexpr nonnull<T *, true>& operator=(U &ref) {
            ptr = static_cast<T *>(&ref);
            assert(ptr != nullptr);
            return *this;
        }

        static constexpr std::result_nt<nonnull<T *, true>> from(T *p) {
            if (p == nullptr) {
                return std::unexpected(std::error_type::NULLPTR);
            }
            return nonnull<T *, true>(p);
        }

        static constexpr std::result_nt<nonnull<T *, true>> from(
            std::nullptr_t) = delete;

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

        template <typename U>
        operator U *() const {
            static_assert(std::is_convertible_v<T *, U *>,
                          "Incompatible pointer type");
            return static_cast<U *>(ptr);
        }
    };

    template <typename T>
    nonnull(T &) -> nonnull<T *, true>;

    template <typename T>
    constexpr std::result_nt<nonnull<T *, true>> nnullfrom(T *p) {
        return nonnull<T *, true>::from(p);
    }

    template <typename T>
    constexpr nonnull<T *, true> nnullforce(T *p) {
        assert(p != nullptr);
        return nonnull<T *, true>::from(p).value();
    }
}  // namespace util