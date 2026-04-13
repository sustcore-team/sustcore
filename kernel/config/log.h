/**
 * @file log.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 日志配置
 * @version alpha-1.0.0
 * @date 2026-04-12
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <config/detector.h>

// default values
#ifndef DEFAULT_LOG_LEVEL

#ifdef __CONF_LOGGER_DISABLE
#define DEFAULT_LOG_LEVEL LogLevel::DISABLE
#else
#define DEFAULT_LOG_LEVEL LogLevel::INFO
#endif

#endif

#ifndef __CONF_LOGGER_LEVEL_SUSTCORE
#define __CONF_LOGGER_LEVEL_SUSTCORE DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_MEMORY
#define __CONF_LOGGER_LEVEL_MEMORY DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_PAGING
#define __CONF_LOGGER_LEVEL_PAGING DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_BUDDY
#define __CONF_LOGGER_LEVEL_BUDDY DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_SLUB
#define __CONF_LOGGER_LEVEL_SLUB DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_DEVICE
#define __CONF_LOGGER_LEVEL_DEVICE DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_INTERRUPT
#define __CONF_LOGGER_LEVEL_INTERRUPT DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_CAPABILITY
#define __CONF_LOGGER_LEVEL_CAPABILITY DEFAULT_LOG_LEVEL
#endif

#ifndef __CONF_LOGGER_LEVEL_TASK
#define __CONF_LOGGER_LEVEL_TASK DEFAULT_LOG_LEVEL
#endif