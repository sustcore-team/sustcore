/**
 * @file macros.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 各种宏技巧
 * @version alpha-1.0.0
 * @date 2026-01-15
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
#define SCMACRO_ARGCHECK(...)                                               \
    _SCMACRO_ARGCHECK(N, ##__VA_ARGS__, _X, _X, _X, _X, _X, _X, _X, _X, _X, \
                      _X, _X, _X, _X, _X, _X, _1, _0)
#define _SCMACRO_ARGCHECK(N, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, \
                          _12, _13, _14, _15, TARGET, ...)                     \
    TARGET

// 多重展开宏, 确保所有参数都被完全展开
#define SCEXP(x)    _SCEXP_1(_SCEXP_1(_SCEXP_1(_SCEXP_1(x))))
#define _SCEXP_1(x) _SCEXP_2(_SCEXP_2(_SCEXP_2(_SCEXP_2(x))))
#define _SCEXP_2(x) _SCEXP_3(_SCEXP_3(_SCEXP_3(_SCEXP_3(x))))
#define _SCEXP_3(x) _SCEXP_4(_SCEXP_4(_SCEXP_4(_SCEXP_4(x))))
#define _SCEXP_4(x) x

#define SCFOREACH(MACRO, ...) SCEXP(_SCFOREACH(MACRO, __VA_ARGS__))
// 利用SCCAT, 根据不同参数个数调用不同的宏实现
#define _SCFOREACH(MACRO, ...) \
    SCCAT(__SCFOREACH, SCMACRO_ARGCHECK(__VA_ARGS__))(MACRO, __VA_ARGS__)
// 辅助 _SCFOREACH_N 的实现
// 原理: SCDEFER将_SCFOREACH展开延迟到下一轮预处理
// 下一轮预处理前, SCDEFER(_SCFOREACH)() (MACRO, __VA_ARGS__)被处理为:
// _SCFOREACH SCEMPTY() (MACRO, __VA_ARGS__)
// 进而被处理为:
// SCFOREACH(MACRO, __VA_ARGS__)
// 从而实现递归调用
#define __SCFOREACH() _SCFOREACH
// 参数个数为0时不展开任何内容
#define __SCFOREACH_0(MACRO, A)
// 参数个数为1时进行一次调用
#define __SCFOREACH_1(MACRO, A) MACRO(A)
#define __SCFOREACH_X(MACRO, A, ...) \
    MACRO(A) SCDEFER(__SCFOREACH)()(MACRO, __VA_ARGS__)

// 删除对象
#define SC_DELETE(x)    kfree(x);
#define SC_DELETES(...) SCFOREACH(SC_DELETE, __VA_ARGS__)

// 添加分号
#define SC_SEMICOLON(x)    x;
#define SC_SEMICOLONS(...) SCFOREACH(SC_SEMICOLON, __VA_ARGS__)

/**
 * @brief 表达式失败时返回指定值
 * @param expr 要检验的某个布尔表达式
 * @param err_ret 失败时返回的值
 * @param ... 需要删除的对象列表
 *
 * 表达式计算结果无法通过检验时, 该宏会删除需要删除的对象,
 * 报错并返回err_ret
 *
 */
#define SC_GUARD(expr, err_ret, ...)                                          \
    do {                                                                      \
        if (!((expr))) {                                                      \
            log_error("%s: 表达式" SCSTRINGIFY(expr) " 失败!", __FUNCTION__); \
            SC_DELETES(__VA_ARGS__)                                           \
            return err_ret;                                                   \
        }                                                                     \
    } while (0)

#define SC_GUARD_EXT(expr, err_ret, ...)                                      \
    do {                                                                      \
        if (!((expr))) {                                                      \
            log_error("%s: 表达式" SCSTRINGIFY(expr) " 失败!", __FUNCTION__); \
            SC_SEMICOLONS(__VA_ARGS__)                                        \
            return err_ret;                                                   \
        }                                                                     \
    } while (0)

/**
 * @brief 表达式失败时返回指定值
 * @param expr 要检验的某个布尔表达式
 * @param err_ret 失败时返回的值
 * @param msg 自定义错误信息
 * @param ... 需要删除的对象列表
 *
 * 表达式计算结果无法通过检验时, 该宏会删除需要删除的对象,
 * 报错并返回err_ret
 *
 */
#define SC_GUARD_MSG(expr, err_ret, msg, ...)      \
    do {                                           \
        if (!((expr))) {                           \
            log_error("%s:%s", __FUNCTION__, msg); \
            SC_DELETES(__VA_ARGS__)                \
            return err_ret;                        \
        }                                          \
    } while (0)

#define SC_NONNULL(ptr, err_ret, ...)                                     \
    do {                                                                  \
        if (ptr == nullptr) {                                             \
            log_error("%s: 指针" SCSTRINGIFY(ptr) "为空!", __FUNCTION__); \
            SC_DELETES(__VA_ARGS__)                                       \
            return err_ret;                                               \
        }                                                                 \
    } while (0)

#define SC_NONNULL_MSG(ptr, err_ret, msg, ...)     \
    do {                                           \
        if (ptr == nullptr) {                      \
            log_error("%s:%s", __FUNCTION__, msg); \
            SC_DELETES(__VA_ARGS__)                \
            return err_ret;                        \
        }                                          \
    } while (0)

#define SC_NONNULL_EXT(ptr, err_ret, ...)                                 \
    do {                                                                  \
        if (ptr == nullptr) {                                             \
            log_error("%s: 指针" SCSTRINGIFY(ptr) "为空!", __FUNCTION__); \
            SC_SEMICOLONS(__VA_ARGS__)                                    \
            return err_ret;                                               \
        }                                                                 \
    } while (0)

#define SC_NEW(type, ptr, err_ret, ...)        \
    type *ptr = (type *)kmalloc(sizeof(type)); \
    SC_NONNULL(ptr, err_ret, __VA_ARGS__);

#define NEW(type, ptr, ...)  SC_NEW(type, ptr, sc_err_ret, __VA_ARGS__)
#define GUARD(expr, ...)     SC_GUARD(expr, sc_err_ret, __VA_ARGS__)
#define GUARD_EXT(expr, ...) SC_GUARD_EXT(expr, sc_err_ret, __VA_ARGS__)
#define NONNULL(ptr, ...)    SC_NONNULL(ptr, sc_err_ret, __VA_ARGS__)
#define NONNULL_MSG(ptr, msg, ...) \
    SC_NONNULL_MSG(ptr, sc_err_ret, msg, __VA_ARGS__)
#define NONNULL_EXT(ptr, ...) SC_NONNULL_EXT(ptr, sc_err_ret, __VA_ARGS__)
#define GUARD_MSG(expr, msg, ...) \
    SC_GUARD_MSG(expr, sc_err_ret, msg, __VA_ARGS__)
#define SET_ERR_RET(type, val) type sc_err_ret = val;