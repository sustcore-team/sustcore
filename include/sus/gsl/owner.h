/**
 * @file owner.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief gsl::owner
 * @version alpha-1.0.0
 * @date 2026-03-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

namespace gsl {
    /**
     * @brief 表示拥有所有权的指针类型
     *
     * @tparam T 指针指向的类型
     */
    template <typename T>
    using owner = T*;
}  // namespace gsl