/**
 * @file ansi.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief ANSI转义序列
 * @version alpha-1.0.0
 * @date 2026-02-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/macros.h>

#define ANSI_ESCAPE "\x1b"  // escape的ASCII码
#define ANSI_CSI    ANSI_ESCAPE "["
#define ANSI_DCS    ANSI_ESCAPE "P"
#define ANSI_OSC    ANSI_ESCAPE "]"

// 参数包
#define __ANSI_ARGP(x)    ";" SCSTRINGIFY(x)
#define __ANSI_ARGPS(...) SCFOREACH(__ANSI_ARGP, __VA_ARGS__)

// 图形模式
#define ANSI_GRAPHIC(m, ...) \
    ANSI_CSI SCSTRINGIFY(m) __ANSI_ARGPS(__VA_ARGS__) "m"

// 图形模式
#define ANSI_GM_RESET      0
#define ANSI_GM_BOLD       1
#define ANSI_GM_ABOLD      22
#define ANSI_GM_BLUR       2
#define ANSI_GM_ABLUR      22
#define ANSI_GM_ITALIC     3
#define ANSI_GM_AITALIC    23
#define ANSI_GM_UNDERLINE  4
#define ANSI_GM_AUNDERLINE 24

// 颜色
#define ANSI_FG_BLACK   30
#define ANSI_FG_RED     31
#define ANSI_FG_GREEN   32
#define ANSI_FG_YELLOW  33
#define ANSI_FG_BLUE    34
#define ANSI_FG_MAGENTA 35
#define ANSI_FG_CYAN    36
#define ANSI_FG_WHITE   37
#define ANSI_FG_DEFAULT 39

#define ANSI_FG_BBLACK   90
#define ANSI_FG_BRED     91
#define ANSI_FG_BGREEN   92
#define ANSI_FG_BYELLOW  93
#define ANSI_FG_BBLUE    94
#define ANSI_FG_BMAGENTA 95
#define ANSI_FG_BCYAN    96
#define ANSI_FG_BWHITE   97

#define ANSI_BG_BLACK   40
#define ANSI_BG_RED     41
#define ANSI_BG_GREEN   42
#define ANSI_BG_YELLOW  43
#define ANSI_BG_BLUE    44
#define ANSI_BG_MAGENTA 45
#define ANSI_BG_CYAN    46
#define ANSI_BG_WHITE   47
#define ANSI_BG_DEFAULT 49

#define ANSI_BG_BBLACK   100
#define ANSI_BG_BRED     101
#define ANSI_BG_BGREEN   102
#define ANSI_BG_BYELLOW  103
#define ANSI_BG_BBLUE    104
#define ANSI_BG_BMAGENTA 105
#define ANSI_BG_BCYAN    106
#define ANSI_BG_BWHITE   107