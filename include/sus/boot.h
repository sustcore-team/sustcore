/**
 * @file boot.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 启动相关函数声明
 * @version alpha-1.0.0
 * @date 2025-11-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

/**
 * @brief 内核启动函数
 * 
 */
void kernel_setup(void);

/**
 * @brief 内核putchar
 * 
 * 应由对应arch实现
 * 
 * @param ch 字符
 * @return int 写入的字符
 */
int kputchar(int ch);

/**
 * @brief 内核puts
 * 
 * 应由对应arch实现
 * 
 * @param str 字符串
 * @return int 写入的字符数
 */
int kputs(const char *str);

/**
 * @brief 内核printf
 * 
 * @param format 格式字符串
 * @param ... 可变参数
 * @return int 写入的字符数
 */
int kprintf(const char *format, ...);
