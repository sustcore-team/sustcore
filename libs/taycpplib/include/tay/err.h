/**
 * @file err.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay C++ 库的错误码。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

namespace tay {
    /**
     * @brief Error codes shared by exception-free tay interfaces.
     */
    enum class error_code {
        NONE,
        OVERFLOW_ERROR,
        UNDERFLOW_ERROR,
        OUT_OF_RANGE,
        NULLPTR,
        INVALID_ARGUMENT,
        OUT_OF_MEMORY,
        ALLOCATION_SIZE_OVERFLOW,
    };

    [[nodiscard]]
    constexpr const char *to_string(error_code code) noexcept {
        switch (code) {
            case error_code::NONE:                     return "NONE";
            case error_code::OVERFLOW_ERROR:           return "OVERFLOW_ERROR";
            case error_code::UNDERFLOW_ERROR:          return "UNDERFLOW_ERROR";
            case error_code::OUT_OF_RANGE:             return "OUT_OF_RANGE";
            case error_code::NULLPTR:                  return "NULLPTR";
            case error_code::INVALID_ARGUMENT:         return "INVALID_ARGUMENT";
            case error_code::OUT_OF_MEMORY:            return "OUT_OF_MEMORY";
            case error_code::ALLOCATION_SIZE_OVERFLOW: return "ALLOCATION_SIZE_OVERFLOW";
        }
        return "<unknown>";
    }
}  // namespace tay
