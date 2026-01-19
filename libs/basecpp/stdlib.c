/**
 * @file stdlib.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief stdlib.h函数实现
 * @version alpha-1.0.0
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <stdlib.h>

static int helper_c2d(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 10;
    } else if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 10;
    } else {
        return -1;
    }
}

// 将str根据传入的base转换为无符号长整数
unsigned long int strtoul(const char *restrict str, char **endptr, int base) {
    const char *p            = str;
    unsigned long int result = 0;
    int digit;

    // 跳过前导空白字符
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v' || *p == '\f' ||
           *p == '\r')
    {
        p++;
    }

    // 处理可选的0x或0前缀
    if (base == 0 || base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            base  = 16;
            p    += 2;
        }
    }
    if (base == 0) {
        if (p[0] == '0') {
            base  = 8;
            p    += 1;
        } else {
            base = 10;
        }
    }

    // 转换数字
    while ((digit = helper_c2d(*p)) >= 0 && digit < base) {
        result = result * base + digit;
        p++;
    }

    // 设置endptr指向转换结束的位置
    if (endptr) {
        *endptr = (char *)p;
    }

    return result;
}