/**
 * @file cxa_dso.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief cxa dso_handle 符号定义
 * @version 0.1.0-dev.1
 * @date 2026-08-09
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cxa/cxa_dso.h>

extern "C" {
void *__dso_handle = static_cast<void *>(&__dso_handle);
}