/**
 * @file detector.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief detector
 * @version alpha-1.0.0
 * @date 2026-04-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#pragma once

#ifndef __SUS_NO_RTTI__
#error \
    "RTTI is not supported in the kernel. Please define __SUS_NO_RTTI__ to compile."
#endif

#ifndef __SUS_NO_EXCEPTIONS__
#error \
    "Exceptions are not supported in the kernel. Please define __SUS_NO_EXCEPTIONS__ to compile."
#endif

#ifdef __SUS_LOGGER_DISABLE__
#define __CONF_LOGGER_DISABLE
#endif

#ifdef __SUS_KERNEL_TESTS__
#define __CONF_KERNEL_TESTS
#define DEFAULT_LOG_LEVEL LogLevel::DEBUG
#endif