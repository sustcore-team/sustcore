/**
 * @file feature.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief features
 * @version alpha-1.0.0
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#ifdef __SUSTCORE_CONFIG__
#include <features/sus/config.h>
#else
#include <features/std/config.h>
#endif

#ifdef __SUS_NO_EXCEPTIONS__
#include <features/sus/exception.h>
#else
#include <features/std/exception.h>
#endif

#include <features/diagnostic.h>