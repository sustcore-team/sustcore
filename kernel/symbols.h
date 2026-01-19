/**
 * @file symbols.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 符号
 * @version alpha-1.0.0
 * @date 2025-11-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

/**
 * @brief 内核起始/结束处位置
 *
 * 注意, 该符号指向该位置, 但该符号本身值不为该位置
 *
 */
extern void *skernel, *ekernel;

/**
 * @brief 文本段起始/结束处位置
 *
 * 注意, 该符号指向该位置, 但该符号本身值不为该位置
 *
 */
extern void *s_text, *e_text;

/**
 * @brief IVT起始/结束处位置
 *
 * 注意, 该符号指向该位置, 但该符号本身值不为该位置
 *
 */
extern void *s_ivt, *e_ivt;

/**
 * @brief 只读数据段起始/结束处位置
 *
 * 注意, 该符号指向该位置, 但该符号本身值不为该位置
 *
 */
extern void *s_rodata, *e_rodata;

/**
 * @brief 数据段起始/结束处位置
 *
 * 注意, 该符号指向该位置, 但该符号本身值不为该位置
 *
 */
extern void *s_data, *e_data;

/**
 * @brief BSS段起始/结束处位置
 *
 * 注意, 该符号指向该位置, 但该符号本身值不为该位置
 *
 */
extern void *s_bss, *e_bss;

/**
 * @brief 内核剩余部分起始处位置
 *
 */
extern void *s_misc;

/**
 * @brief 内核附加文件:license 起始/结束处位置
 *
 */
extern void *s_attach_license, *e_attach_license;

/**
 * @brief 内核附加文件:test.mod 起始/结束处位置
 *
 */
extern void *s_attach_test, *e_attach_test;

/**
 * @brief 存放临时用户态代码
 * 
 */
extern void *s_ptest1, *e_ptest1;