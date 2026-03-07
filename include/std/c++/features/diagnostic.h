/**
 * @file diagnostic.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 诊断消息控制
 * @version alpha-1.0.0
 * @date 2026-03-07
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#if defined(__clang__)
#define SUPPRESS_WARNING_BEGIN _Pragma("clang diagnostic push")
#define SUPPRESS_WARNING_END   _Pragma("clang diagnostic pop")
#define SUPPRESS_USER_DEFINED_LITERAL_SUFFIX \
    _Pragma("clang diagnostic ignored \"-Wuser-defined-literals\"")
#define SUPPRESS_SELF_MOVE _Pragma("clang diagnostic ignored \"-Wself-move\"")
#define SUPPRESS_SELF_ASSIGN \
    _Pragma("clang diagnostic ignored \"-Wself-assign-overloaded\"")
#elif defined(__GNUC__) || defined(__GNUG__) p
#define SUPPRESS_WARNING_BEGIN _Pragma("GCC diagnostic push")
#define SUPPRESS_WARNING_END   _Pragma("GCC diagnostic pop")
#define SUPPRESS_USER_DEFINED_LITERAL_SUFFIX \
    _Pragma("GCC diagnostic ignored \"-Wliteral-suffix\"")
#define SUPPRESS_SELF_MOVE _Pragma("GCC diagnostic ignored \"-Wself-move\"")
#define SUPPRESS_SELF_ASSIGN
#else
#warning "Failed at suppressing diagnostic infomations!"
// 其他编译器中不处理该警告
#define SUPPRESS_WARNING_BEGIN
#define SUPPRESS_WARNING_END
#define SUPPRESS_USER_DEFINED_LITERAL_SUFFIX
#define SUPPRESS_SELF_MOVE
#endif