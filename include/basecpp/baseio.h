/**
 * @file baseio.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief primitive io 头文件
 * @version alpha-1.0.0
 * @date 2023-04-08
 *
 * @copyright Copyright (c) 2022 TayhuangOS Development Team
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#pragma once

#include <concept>
#include <cstdarg>
#include <cstdio>


namespace basecpp {

    template <typename T>
    concept IOTrait = requires(char c, const char *str) {
        {
            T::putchar(c)
        } -> std::same_as<int>;
        {
            T::puts(str)
        } -> std::same_as<int>;
        {
            T::getchar()
        } -> std::same_as<char>;
    };

}  // namespace basecpp

/**
 * @brief 输出到chan对应的IO设备上
 *
 * @tparam T IO频道类型
 * @param fmt 格式化字符串
 * @param args 参数
 * @return 输出字符数
 */
template <basecpp::IOTrait T>
int vbprintf(const char *fmt, va_list args) {
    char buffer[1024];
    int len = vsprintf(buffer, fmt, args);
    T::puts(buffer);
    return len;
}

/**
 * @brief 输出到buffer中
 *
 * @param buffer 缓存
 * @param fmt 格式化字符串
 * @param args 参数
 * @return 输出字符数
 */
int vsprintf(char *buffer, const char *fmt, va_list args);

/**
 * @brief 输出到chan对应的IO设备上
 *
 * @tparam T IO频道类型
 * @param fmt 格式化字符串
 * @param ... 参数
 * @return 输出字符数
 */
template <basecpp::IOTrait T>
int bprintf(const char *fmt, ...) {
    va_list lst;
    va_start(lst, fmt);
    int ret = vbprintf<T>(fmt, lst);
    va_end(lst);
    return ret;
}

/**
 * @brief 输出到buffer中
 *
 * @param buffer 缓存
 * @param fmt 格式化字符串
 * @param ... 参数
 * @return 输出字符数
 */
int sprintf(char *buffer, const char *fmt, ...);