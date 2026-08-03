/**
 * @file cmp.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 提供比较器和比较适配工具。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <utility>

namespace tay::__algo {
    struct __less {
        template <class L, class R>
        [[nodiscard]]
        constexpr bool operator()(L&& lhs, R&& rhs) const {
            return std::forward<L>(lhs) < std::forward<R>(rhs);
        }
    };

    struct __greater {
        template <class L, class R>
        [[nodiscard]]
        constexpr bool operator()(L&& lhs, R&& rhs) const {
            return std::forward<R>(rhs) < std::forward<L>(lhs);
        }
    };

    struct __equal_to {
        template <class L, class R>
        [[nodiscard]]
        constexpr bool operator()(L&& lhs, R&& rhs) const {
            return std::forward<L>(lhs) == std::forward<R>(rhs);
        }
    };
}  // namespace tay::__algo