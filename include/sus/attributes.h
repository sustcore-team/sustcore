/**
 * @file attributes.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief attributes定义
 * @version alpha-1.0.0
 * @date 2025-11-19
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

/**
 * @brief PACKED属性
 *
 * 使结构体按紧凑方式对齐
 */
#define PACKED __attribute__((packed))

/**
 * @brief ALIGNED属性
 *
 * 指定变量或结构体的对齐方式
 */
#define ALIGNED(x) __attribute__((aligned(x)))

/**
 * @brief NAKED属性
 *
 * 指示编译器不生成函数的前导和尾随代码
 */
#define NAKED __attribute__((naked))

/**
 * @brief SECTION属性
 *
 * 指示编译器将变量或函数放置在指定的段中
 *
 */
#define SECTION(x) __attribute__((section(#x)))