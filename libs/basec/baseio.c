/**
 * @file baseio.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief primitive io - 实现
 * @version alpha-1.0.0
 * @date 2023-04-08
 *
 * @copyright Copyright (c) 2022 TayhuangOS Development Team
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <basec/baseio.h>
#include <basec/logger.h>
#include <basec/tostring.h>
#include <ctype.h>
#include <string.h>

/**
 * @brief 底层printf
 *
 * @param buffer 输出缓冲
 * @param fmt 格式化字符串
 * @param args 参数
 * @return 输出字符数
 */
static int __llprintf(char *buffer, const char *fmt, va_list args) {
    enum {
        // 标志
        FLAG_SIGN         = 1 << 0,  // 显示符号
        FLAG_LEFT_ALIGN   = 1 << 1,  // 左对齐
        FLAG_FILL_ZERO    = 1 << 2,  // 用0填充
        FLAG_PREFIX       = 1 << 3,  // 前缀0x/0
        FLAG_UPPER        = 1 << 4,  // 大写十六进制
        // 打印类型
        PRINT_TY_INT      = 0,  // 整型
        PRINT_TY_UNSIGNED = 1,  // 无符号整型
        PRINT_TY_OCT      = 2,  // 八进制
        PRINT_TY_HEX      = 3,  // 十六进制
        PRINT_TY_CHAR     = 4,  // 字符
        PRINT_TY_STRING   = 5,  // 字符串
        // 位长
        QUAL_SHORT        = 0,  // 短
        QUAL_NORMAL       = 1,  // 普通
        QUAL_LONG         = 2   // 长
    };

    // 保存原指针
    char *original = buffer;

    // fmt未结束
    while (*fmt) {
        // 非格式化字符
        if (*fmt != '%') {
            // 直接加入
            *buffer = *fmt;
            buffer++;
            fmt++;
        } else {
            // 格式化
            fmt++;

            // %%
            if (*fmt == '%') {
                *buffer = '%';
                buffer++;
                fmt++;
                continue;
            }

            // 格式化信息
            unsigned char flag       = 0;
            unsigned int width       = 0;
            unsigned int precision   = 0;
            unsigned char print_type = PRINT_TY_INT;
            unsigned char qualifier  = QUAL_NORMAL;

            // 特殊标记(+ - 0 #)
            while (true) {
                bool _break = false;
                switch (*fmt) {
                    case '+':
                        flag |= FLAG_SIGN;
                        fmt++;
                        break;
                    case '-':
                        flag |= FLAG_LEFT_ALIGN;
                        fmt++;
                        break;
                    case ' ': fmt++; break;
                    case '0':
                        flag |= FLAG_FILL_ZERO;
                        fmt++;
                        break;
                    case '#':
                        flag |= FLAG_PREFIX;
                        fmt++;
                        break;
                    default: _break = true; break;  // 标记结束
                }
                if (_break) {
                    break;
                }
            }

            // 对齐标记
            // *说明放在可变参数表中
            if (*fmt == '*') {
                width = va_arg(args, int);
                // 负数说明左对齐
                if (width < 0) {
                    width  = -width;
                    flag  |= FLAG_LEFT_ALIGN;
                }
                fmt++;
            } else {
                // 获取对齐宽度
                while (isdigit(*fmt)) {
                    width = width * 10 + *fmt - '0';
                    fmt++;
                }
            }

            // 精度标记
            // *说明放在可变参数表中
            if (*fmt == '.') {
                fmt++;
                if (*fmt == '*') {
                    precision = va_arg(args, int);
                    fmt++;
                } else {
                    // 获取精度
                    while (isdigit(*fmt)) {
                        precision = precision * 10 + *fmt - '0';
                        fmt++;
                    }
                }
            }

            // 精度拓展标记
            switch (*fmt) {
                case 'l':
                case 'L':
                    qualifier = QUAL_LONG;
                    fmt++;
                    break;
                case 'h':
                    qualifier = QUAL_SHORT;
                    fmt++;
                    break;
            }

            // 类型标记
            switch (*fmt) {
                case 'd': print_type = PRINT_TY_INT; break;
                case 'u': print_type = PRINT_TY_UNSIGNED; break;
                case 'o': print_type = PRINT_TY_OCT; break;
                case 'X': flag |= FLAG_UPPER;  // 大写十六进制
                case 'x': print_type = PRINT_TY_HEX; break;
                case 'c': print_type = PRINT_TY_CHAR; break;
                case 's': print_type = PRINT_TY_STRING; break;
                case 'P': flag |= FLAG_UPPER;  // 大写指针
                case 'p':
                    flag       |= FLAG_FILL_ZERO;
                    flag       |= FLAG_PREFIX;
                    width       = 16;
                    print_type  = PRINT_TY_HEX;
                    break;  // 指针
                default: print_type = -1; break;
            }

            // 解析失败
            if (print_type == -1) {
                return -1;
            }
            fmt++;

            // 符号标记和缓存
            bool has_sign     = false;
            int offset        = 0;
            char _buffer[120] = {};

            switch (print_type) {
                // 整型
                case PRINT_TY_INT: {
                    if (qualifier == QUAL_LONG || qualifier == QUAL_NORMAL ||
                        qualifier == QUAL_SHORT)
                    {
                        unsigned long long val = va_arg(args, unsigned long long);
                        // 小于0则取相反数并设置符号标记
                        if (val < 0) {
                            has_sign = true;
                            val      = -val;
                        }
                        lltoa(val, _buffer, 10);
                    }
                    break;
                }
                // 无符号整型
                case PRINT_TY_UNSIGNED: {
                    if (qualifier == QUAL_LONG || qualifier == QUAL_NORMAL ||
                        qualifier == QUAL_SHORT)
                    {
                        unsigned long long val = va_arg(args, unsigned long long);
                        ulltoa(val, _buffer, 10);
                    }
                    break;
                }
                // 八进制
                case PRINT_TY_OCT: {
                    if (qualifier == QUAL_LONG || qualifier == QUAL_NORMAL ||
                        qualifier == QUAL_SHORT)
                    {
                        unsigned long long val = va_arg(args, unsigned long long);
                        ulltoa(val, _buffer, 8);
                    }
                    break;
                }
                // 十六进制
                case PRINT_TY_HEX: {
                    if (qualifier == QUAL_LONG || qualifier == QUAL_NORMAL ||
                        qualifier == QUAL_SHORT)
                    {
                        unsigned long long val = va_arg(args, unsigned long long);
                        ulltoa(val, _buffer, 16);

                        // 若大写
                        if (flag & FLAG_UPPER) {
                            char *__buffer = _buffer;
                            while (*__buffer != '\0') {
                                if (islower(*__buffer)) {
                                    *__buffer = toupper(*__buffer);
                                }
                                __buffer++;
                            }
                        }
                    }
                    break;
                }
                // 字符
                case PRINT_TY_CHAR: {
                    char ch = (char)va_arg(args, unsigned int);
                    *buffer = ch;
                    buffer++;
                    offset++;
                    break;
                }
                // 字符串
                case PRINT_TY_STRING: {
                    char *str = va_arg(args, char *);
                    strcpy(buffer, str);
                    buffer += strlen(str);
                    offset += strlen(str);
                    break;
                }
            }

            // 符号标记
            if (flag & FLAG_SIGN) {
                // 是否为负数
                if (!has_sign) {
                    // 添加正号
                    *buffer = '+';
                    buffer++;
                    offset++;
                }
            }

            // 是否为负数
            if (has_sign) {
                // 添加负号
                *buffer = '-';
                buffer++;
                offset++;
            }

            // 前缀
            if (flag & FLAG_PREFIX) {
                // 八进制
                if (print_type == PRINT_TY_OCT) {
                    *buffer = '0';
                    buffer++;
                } else if (print_type == PRINT_TY_HEX) {
                    // 十六进制
                    *buffer = '0';
                    buffer++;
                    *buffer = flag & FLAG_UPPER ? 'X' : 'x';
                    buffer++;
                }
            }

            // 填充字符
            char fillch = flag & FLAG_FILL_ZERO ? '0' : ' ';

            // 填充
            for (int i = strlen(_buffer) + offset; i < width; i++) {
                *buffer = fillch;
                buffer++;
            }

            // 添加到buffer中
            strcpy(buffer, _buffer);
            // 移动指针
            buffer += strlen(_buffer);
        }
    }

    // 结束标记
    *buffer = '\0';

    return buffer - original;
    return 0;
}

/**
 * @brief 使用bputs输出
 *
 * @param bputs 输出函数
 * @param fmt 格式化字符串
 * @param args 参数
 * @return 输出字符数
 */
int vbprintf(BaseCPutsFunc bputs, const char *fmt, va_list args) {
    char buffer[512];

    int ret = __llprintf(buffer, fmt, args);
    bputs(buffer);

    return ret;
}

/**
 * @brief 输出到buffer中
 *
 * @param buffer 缓存
 * @param fmt 格式化字符串
 * @param args 参数
 * @return 输出字符数
 */
int vsprintf(char *buffer, const char *fmt, va_list args) {
    return __llprintf(buffer, fmt, args);
}

/**
 * @brief 使用bputs输出
 *
 * @param bputs 输出函数
 * @param fmt 格式化字符串
 * @param ... 参数
 * @return 输出字符数
 */
int bprintf(BaseCPutsFunc bputs, const char *fmt, ...) {
    // 初始化可变参数
    va_list lst;
    va_start(lst, fmt);

    int ret = vbprintf(bputs, fmt, lst);

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
int sprintf(char *buffer, const char *fmt, ...) {
    // 初始化可变参数
    va_list lst;
    va_start(lst, fmt);

    int ret = vsprintf(buffer, fmt, lst);

    va_end(lst);
    return ret;
}

int ssprintf(char *str, const char *fmt, ...) {
    // 初始化可变参数
    va_list lst;
    va_start(lst, fmt);

    int ret = __llprintf(str, fmt, lst);

    va_end(lst);
    return ret;
}

// /**
//  * @brief 使用bgetchar输入
//  *
//  * @param bgetchar 输入函数
//  * @param fmt 格式化字符串
//  * @param args 参数
//  * @return 输入字符数
//  */
// int vbscanf(BaseCGetcharFunc bgetchar, const char *fmt, va_list args) {
//     // TODO
//     return 0;
// }

// /**
//  * @brief 使用bgetchar输入
//  *
//  * @param bgetchar 输入函数
//  * @param fmt 格式化字符串
//  * @param ... 参数
//  * @return 输入字符数
//  */
// int bscanf(BaseCGetcharFunc bgetchar, const char *fmt, ...) {
//     // 初始化可变参数
//     va_list lst;
//     va_start(lst, fmt);

//     int ret = vbscanf(bgetchar, fmt, lst);

//     va_end(lst);
//     return ret;
// }