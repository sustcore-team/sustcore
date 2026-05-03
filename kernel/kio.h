/**
 * @file kio.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 输入输出
 * @version alpha-1.0.0
 * @date 2026-01-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <sus/baseio.h>

int kputs(const char* str);
int kwrites(const char* str, size_t len);
int kputchar(char ch);
char kgetchar();
int kprintf(const char* fmt, ...);
int kprintfln(const char* fmt, ...);

struct KernelIO {
    static int putchar(char c);
    static int puts(const char* str);
    static char getchar();
};

constexpr KernelIO kio;

static_assert(basecpp::IOTrait<KernelIO>, "KernelIO does not satisfy IOTrait");

// Loggers
#include <config/log.h>
#include <sus/logger.h>

namespace loggers {
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_SUSTCORE, SUSTCORE);
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_MEMORY, MEMORY)
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_PAGING, PAGING);
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_BUDDY, BUDDY)
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_SLUB, SLUB);
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_DEVICE, DEVICE)
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_INTERRUPT, INTERRUPT);
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_CAPABILITY, CAPABILITY);
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_TASK, TASK);
    DECLARE_LOGGER(KernelIO, __CONF_LOGGER_LEVEL_SYSCALL, SYSCALL);
};  // namespace loggers