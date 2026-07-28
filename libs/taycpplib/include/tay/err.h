/**
 * @file err.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Error codes
 * @version 0.1.0-dev.1
 * @date 2026-07-28
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
    };
}  // namespace tay
