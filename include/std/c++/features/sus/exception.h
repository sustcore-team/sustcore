/**
 * @file exception.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief exception
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <bits/exception.h>
#include <features/sus/errors.h>
#include <cassert>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define _THROW(exception) panic(#exception)
#define _TRY
#define _CATCH(exception) if (0)
#define EXCEPTION_DEPRECATED [[deprecated("此函数不应被调用!其使用了异常机制, "                                     \
        "但内核中不应使用该机制!请调用其对应的无异常版本(一般命名为 " \
        "xxx_nt)!")]]
// NOLINTEND(cppcoreguidelines-macro-usage)
