/**
 * @file macros.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 定义 Tay C 库通用的预处理器和类型工具宏。
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

// 延迟展开
#define SCEMPTY()
#define SCDEFER(x) x SCEMPTY()

// 基本操作
#define SCSTRINGIFY(x)  _SCSTRINGIFY(x)
#define _SCSTRINGIFY(x) #x

#define SCCAT(a, b)  _SCCAT(a, b)
#define _SCCAT(a, b) a##b

// 当参数个数为1时返回_1
// 当参数个数大于1且小于16时返回_X
#define SCMACRO_ARGCHECK(...)                                                                   \
    _SCMACRO_ARGCHECK(N, ##__VA_ARGS__, _X, _X, _X, _X, _X, _X, _X, _X, _X, _X, _X, _X, _X, _X, \
                      _X, _1, _0)
#define _SCMACRO_ARGCHECK(N, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, \
                          TARGET, ...)                                                             \
    TARGET

#define SCMACRO_ARGNUMBER(...)                                                                     \
    _SCMACRO_ARGCHECK(N, ##__VA_ARGS__, _16, _15, _14, _13, _12, _11, _10, _9, _8, _7, _6, _5, _4, \
                      _3, _2, _1, _0)

// 多重展开宏, 确保所有参数都被完全展开
#define SCEXP(x)    _SCEXP_1(_SCEXP_1(_SCEXP_1(_SCEXP_1(x))))
#define _SCEXP_1(x) _SCEXP_2(_SCEXP_2(_SCEXP_2(_SCEXP_2(x))))
#define _SCEXP_2(x) _SCEXP_3(_SCEXP_3(_SCEXP_3(_SCEXP_3(x))))
#define _SCEXP_3(x) _SCEXP_4(_SCEXP_4(_SCEXP_4(_SCEXP_4(x))))
#define _SCEXP_4(x) x

#define SCFOREACH(MACRO, ...)  SCEXP(_SCFOREACH(MACRO, __VA_ARGS__))
// 利用SCCAT, 根据不同参数个数调用不同的宏实现
#define _SCFOREACH(MACRO, ...) SCCAT(__SCFOREACH, SCMACRO_ARGCHECK(__VA_ARGS__))(MACRO, __VA_ARGS__)
// 辅助 _SCFOREACH_N 的实现
// 原理: SCDEFER将_SCFOREACH展开延迟到下一轮预处理
// 下一轮预处理前, SCDEFER(_SCFOREACH)() (MACRO, __VA_ARGS__)被处理为:
// _SCFOREACH SCEMPTY() (MACRO, __VA_ARGS__)
// 进而被处理为:
// SCFOREACH(MACRO, __VA_ARGS__)
// 从而实现递归调用
#define __SCFOREACH()          _SCFOREACH
// 参数个数为0时不展开任何内容
#define __SCFOREACH_0(MACRO, A)
// 参数个数为1时进行一次调用
#define __SCFOREACH_1(MACRO, A)      MACRO(A)
#define __SCFOREACH_X(MACRO, A, ...) MACRO(A) SCDEFER(__SCFOREACH)()(MACRO, __VA_ARGS__)

// 添加分号
#define SC_SEMICOLON(x)    x;
#define SC_SEMICOLONS(...) SCFOREACH(SC_SEMICOLON, __VA_ARGS__)