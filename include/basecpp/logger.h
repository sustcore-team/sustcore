/**
 * @file logger.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief logger 头文件
 * @version alpha-1.0.0
 * @date 2023-4-3
 *
 * @copyright Copyright (c) 2022 TayhuangOS Development Team
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#pragma once

#include <basecpp/baseio.h>

#include <cstdio>

enum class LogLevel { DEBUG = 0, INFO, WARN, ERROR, FATAL };

static constexpr LogLevel GLOBAL_LOG_LEVEL = LogLevel::DEBUG;

constexpr const char *level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

namespace basecpp {
    template <typename T>
    concept LogInfo = requires() {
        {
            T::name
        } -> std::convertible_to<const char *>;
        {
            T::level
        } -> std::same_as<const LogLevel &>;
    };
}  // namespace basecpp

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
class Logger {
private:
    template <LogLevel level, typename... Args>
    void __log__(const char *file, const int line, const char *func,
                 const char *fmt, Args... args);

    template <LogLevel level, typename... Args>
    void __log__(const char *fmt, Args... args);

public:
    template <typename... Args>
    void debug(const char *file, const int line, const char *func,
               const char *fmt, Args... args);

    template <typename... Args>
    void info(const char *file, const int line, const char *func,
              const char *fmt, Args... args);

    template <typename... Args>
    void warn(const char *file, const int line, const char *func,
              const char *fmt, Args... args);

    template <typename... Args>
    void error(const char *file, const int line, const char *func,
               const char *fmt, Args... args);

    template <typename... Args>
    void fatal(const char *file, const int line, const char *func,
               const char *fmt, Args... args);

};

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <LogLevel level, typename... Args>
void Logger<IOChannel, LogInfo>::__log__(const char *file, const int line,
                                         const char *func, const char *fmt,
                                         Args... args) {
    if constexpr (level < GLOBAL_LOG_LEVEL) {
        return;
    }

    if constexpr (level < LogInfo::level) {
        return;
    }

    char buffer[256];
    int offset = 0;

    offset += sprintf(buffer + offset, "[%s:%d:%s] ", file, line, func);

    offset += sprintf(buffer + offset, "[%s]/[%s]: ", level_to_string(level),
                      LogInfo::name);

    offset += sprintf(buffer + offset, fmt, args...);
    offset += sprintf(buffer + offset, "\n");
    IOChannel::puts(buffer);
}

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <LogLevel level, typename... Args>
void Logger<IOChannel, LogInfo>::__log__(const char *fmt, Args... args) {
    if constexpr (level < GLOBAL_LOG_LEVEL) {
        return;
    }

    if constexpr (level < LogInfo::level) {
        return;
    }

    char buffer[256];
    int offset = 0;

    offset += sprintf(buffer + offset, "[%s]/[%s]: ", level_to_string(level),
                      LogInfo::name);

    offset += sprintf(buffer + offset, fmt, args...);
    offset += sprintf(buffer + offset, "\n");
    IOChannel::puts(buffer);
}

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <typename... Args>
void Logger<IOChannel, LogInfo>::debug(const char *file, const int line,
                                       const char *func, const char *fmt,
                                       Args... args) {
    __log__<LogLevel::DEBUG>(file, line, func, fmt, args...);
}

#define debug(fmt, ...) \
    debug(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <typename... Args>
void Logger<IOChannel, LogInfo>::info(const char *file, const int line,
                                      const char *func, const char *fmt,
                                      Args... args) {
    __log__<LogLevel::INFO>(file, line, func, fmt, args...);
}

#define info(fmt, ...) \
    info(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <typename... Args>
void Logger<IOChannel, LogInfo>::warn(const char *file, const int line,
                                      const char *func, const char *fmt,
                                      Args... args) {
    __log__<LogLevel::WARN>(file, line, func, fmt, args...);
}

#define warn(fmt, ...) \
    warn(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <typename... Args>
void Logger<IOChannel, LogInfo>::error(const char *file, const int line,
                                       const char *func, const char *fmt,
                                       Args... args) {
    __log__<LogLevel::ERROR>(file, line, func, fmt, args...);
}

#define error(fmt, ...) \
    error(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

template <basecpp::IOTrait IOChannel, basecpp::LogInfo LogInfo>
template <typename... Args>
void Logger<IOChannel, LogInfo>::fatal(const char *file, const int line,
                                       const char *func, const char *fmt,
                                       Args... args) {
    __log__<LogLevel::FATAL>(file, line, func, fmt, args...);
}

#define fatal(fmt, ...) \
    fatal(__FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)


#define DECLARE_LOGGER(IOChannel, _logLevel, loggerName)       \
    struct loggerName##Logger {                                \
        static constexpr const char name[] = #loggerName;      \
        static constexpr LogLevel level    = _logLevel;        \
    };                                                         \
    static_assert(basecpp::LogInfo<loggerName##Logger>,        \
                  #loggerName " Logger static assert failed"); \
    static Logger<IOChannel, loggerName##Logger> logger;