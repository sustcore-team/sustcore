/**
 * @file ctype.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ctype.h
 * @version alpha-1.0.0
 * @date 2022-12-31
 *
 * @copyright Copyright (c) 2022 TayhuangOS Development Team
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#pragma once

#ifdef __cplusplus
#define restrict __restrict__
extern "C" {
#endif

/**
 * @brief 判断字符是否为空白字符（空格、制表符等）
 * @param ch 要判断的字符
 * @return 非零表示是空白字符，零表示不是
 */
int isspace(int ch);

/**
 * @brief 判断字符是否为大写字母
 * @param ch 要判断的字符
 * @return 非零表示是大写字母，零表示不是
 */
int isupper(int ch);

/**
 * @brief 判断字符是否为小写字母
 * @param ch 要判断的字符
 * @return 非零表示是小写字母，零表示不是
 */
int islower(int ch);

/**
 * @brief 判断字符是否为字母
 * @param ch 要判断的字符
 * @return 非零表示是字母，零表示不是
 */
int isalpha(int ch);

/**
 * @brief 判断字符是否为数字
 * @param ch 要判断的字符
 * @return 非零表示是数字，零表示不是
 */
int isdigit(int ch);

/**
 * @brief 判断字符是否为数字或字母
 * @param ch 要判断的字符
 * @return 非零表示是数字或字母，零表示不是
 */
int isalnum(int ch);

/**
 * @brief 判断字符是否为空白字符
 * @param ch 要判断的字符
 * @return 非零表示是空白字符，零表示不是
 */
int isblank(int ch);

/**
 * @brief 判断字符是否为控制字符
 * @param ch 要判断的字符
 * @return 非零表示是控制字符，零表示不是
 */
int iscntrl(int ch);

/**
 * @brief 判断字符是否为可打印字符
 * @param ch 要判断的字符
 * @return 非零表示是可打印字符，零表示不是
 */
int isprint(int ch);

/**
 * @brief 判断字符是否为可打印字符(非空格)
 * @param ch 要判断的字符
 * @return 非零表示是可打印字符(非空格)，零表示不是
 */
int isgraph(int ch);

/**
 * @brief 判断字符是否为标点符号
 * @param ch 要判断的字符
 * @return 非零表示是标点符号，零表示不是
 */
int ispunct(int ch);

/**
 * @brief 判断字符是否为十六进制字符
 * @param ch 要判断的字符
 * @return 非零表示是十六进制字符，零表示不是
 */
int isxdigit(int ch);

/**
 * @brief 判断字符是否为八进制字符
 * @param ch 要判断的字符
 * @return 非零表示是八进制字符，零表示不是
 */
int isodigit(int ch);

/**
 * @brief 将大写字母转换为小写字母
 * @param ch 要转换的字符
 * @return 转换后的字符
 */
int tolower(int ch);

/**
 * @brief 将小写字母转换为大写字母
 * @param ch 要转换的字符
 * @return 转换后的字符
 */
int toupper(int ch);

#ifdef __cplusplus
}
#undef restrict
#endif