/**
 * @file ctype.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ctypes.h函数实现
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <ctype.h>

/** 是否为空格字符 */
int isspace(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == '\v' ||
           ch == '\f';
}

/** 是否为大写字母 */
int isupper(int ch) {
    return ch >= 'A' && ch <= 'Z';
}

/** 是否为小写字母 */
int islower(int ch) {
    return ch >= 'a' && ch <= 'z';
}

/** 是否为字母 */
int isalpha(int ch) {
    return isupper(ch) || islower(ch);
}

/** 是否为数字 */
int isdigit(int ch) {
    return ch >= '0' && ch <= '9';
}

/** 是否为数字或字母 */
int isalnum(int ch) {
    return isalpha(ch) || isdigit(ch);
}

/** 是否为空白字符 */
int isblank(int ch) {
    return ch == ' ' || ch == '\t';
}

/** 是否为控制字符 */
int iscntrl(int ch) {
    return ((ch >= 0x00 && ch <= 0x1f) || ch == 0x7f);
}

/** 是否为可打印字符 */
int isprint(int ch) {
    return (!iscntrl(ch));
}

/** 是否为可打印字符(非空格) */
int isgraph(int ch) {
    return isprint(ch) && (!isspace(ch));
}

/** 是否为标点符号 */
int ispunct(int ch) {
    return isgraph(ch) && (!isalnum(ch));
}

/** 是否为十六进制字符 */
int isxdigit(int ch) {
    return isalnum(ch) ||
           ((ch >= 'a' && ch <= 'F') || (ch >= 'A' && ch <= 'F'));
}

/** 是否为八进制字符 */
int isodigit(int ch) {
    return ch >= '0' && ch <= '7';
}

/** 大写转小写 */
int tolower(int ch) {
    return isupper(ch) ? (ch - 'A' + 'a') : (ch);
}

/** 小写转大写 */
int toupper(int ch) {
    return islower(ch) ? (ch - 'a' + 'A') : (ch);
}