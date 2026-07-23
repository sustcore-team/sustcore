/**
 * @file exception.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief exception
 * @version alpha-1.0.0
 * @date 2026-03-02
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#include <cstddef>

namespace std {
#ifdef __EXCEPTIONS
    class exception {
    public:
        constexpr exception() noexcept                            = default;
        virtual ~exception() noexcept                             = default;
        constexpr exception(const exception&) noexcept            = default;
        constexpr exception& operator=(const exception&) noexcept = default;
        constexpr exception(exception&&) noexcept                 = default;
        constexpr exception& operator=(exception&&) noexcept      = default;
        [[nodiscard]]
        virtual const char* what() const {
            return "std::exception";
        }
    };
#endif
}  // namespace std