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

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define _THROW(exception) throw exception
#define _TRY              try
#define _CATCH(exception) catch (exception)
#define EXCEPTION_DEPRECATED
// NOLINTEND(cppcoreguidelines-macro-usage)