/**
 * @file bytes.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 无运行时依赖的启动期内存操作接口
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

/**
 * @brief 在启动期复制两个有效且不重叠的字节区间。
 * @note 该函数位于可回收的 init text 中，回收 init 内存后不得调用。
 */
extern "C" void __early_copy(void *dst, const void *src, size_t sz) noexcept;
extern "C" void __early_clear(void *begin, void *end) noexcept;
