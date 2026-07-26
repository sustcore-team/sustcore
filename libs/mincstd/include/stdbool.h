/**
 * @file stdbool.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief stdbool.h
 * @version 0.1.0-dev.1
 * @date 2025-11-18
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define bool _Bool
#define true 1
#define false 0
#endif

#define __bool_true_false_are_defined 1
