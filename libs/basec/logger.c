/**
 * @file logger.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief logger - 实现
 * @version alpha-1.0.0
 * @date 2023-4-3
 *
 * @copyright Copyright (c) 2022 TayhuangOS Development Team
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <basec/baseio.h>
#include <basec/logger.h>
#include <stddef.h>

// 日志名
static const char *logger_name = nullptr;

// 输出流
static BaseCPutsFunc logger_puts;

void init_logger(BaseCPutsFunc bputs, const char *name) {
    bprintf(bputs, "[BCL Logger/INFO]为[%s]初始化日志器中.\n", name);
    // 初始化
    logger_name = name;
    logger_puts = bputs;
}

// 最初级log
void __llog__(const char *name, const char *level, const char *msg) {
    bprintf(logger_puts, "[%s/%s]%s\n", name, level, msg);
}

/**
 * @brief 输出日志
 *
 * @param level 日志等级
 * @param msg 日志消息
 */
static void __log__(LogLevel level, const char *msg) {
    // 获取日志等级
    switch (level) {
        case INFO: {
            __llog__(logger_name, "INFO", msg);
            return;
        }
        case WARNING: {
            __llog__(logger_name, "WARNING", msg);
            return;
        }
        case ERROR: {
            __llog__(logger_name, "ERROR", msg);
            return;
        }
        case FATAL: {
            __llog__(logger_name, "FATAL", msg);
            return;
        }
        case DEBUG: {
            __llog__(logger_name, "DEBUG", msg);
            return;
        }
    }
    __llog__(logger_name, "UNKNOWN", msg);
}

/**
 * @brief 输出日志
 *
 * @param level 日志等级
 * @param fmt 日志消息格式化字符串
 * @param args 日志消息参数
 */
static void __vlog__(LogLevel level, const char *fmt, va_list args) {
    char buffer[512];

    vsprintf(buffer, fmt, args);

    __log__(level, buffer);
}

// 各种等级的Log函数实现

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    __vlog__(INFO, fmt, args);

    va_end(args);
}

void log_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    __vlog__(WARNING, fmt, args);

    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    __vlog__(ERROR, fmt, args);

    va_end(args);
}

void log_fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    __vlog__(FATAL, fmt, args);

    va_end(args);
}

void log_debug(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    __vlog__(DEBUG, fmt, args);

    va_end(args);
}