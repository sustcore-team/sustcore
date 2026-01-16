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

#include <stdarg.h>
#include <stdio.h>

typedef int iochan_t;

/**
 * @brief 将字符打印到chan对应的IO设备上
 * 
 * 由用户实现
 * 
 * @param chan IO通道
 * @param c 要打印的字符
 * @return int 打印的字符数
 */
int basec_putchar(iochan_t chan, char c);

/**
 * @brief 从chan对应的IO设备上读取一个字符
 * 
 * 由用户实现
 * 
 * @param chan IO通道
 * @return char 读取的字符
 */
int basec_puts(iochan_t chan, const char *str);

/**
 * @brief 从chan对应的IO设备上读取一个字符
 * 
 * 由用户实现
 * 
 * @param chan IO通道
 * @return char 读取的字符
 */
char basec_getchar(iochan_t chan);


/**
 * @brief 输出到chan对应的IO设备上
 *
 * @param chan 输出频道
 * @param fmt 格式化字符串
 * @param args 参数
 * @return 输出字符数
 */
int vbprintf(iochan_t chan, const char *fmt, va_list args);

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
 * @param chan 输出频道
 * @param fmt 格式化字符串
 * @param ... 参数
 * @return 输出字符数
 */
int bprintf(iochan_t chan, const char *fmt, ...);

/**
 * @brief 输出到buffer中
 *
 * @param buffer 缓存
 * @param fmt 格式化字符串
 * @param ... 参数
 * @return 输出字符数
 */
int sprintf(char *buffer, const char *fmt, ...);