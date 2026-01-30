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

#include <basecpp/baseio.h>

int kputs(const char* str);
int kputchar(char ch);
char kgetchar();
int kprintf(const char* fmt, ...);

struct KernelIO {
    static int putchar(char c);
    static int puts(const char* str);
    static char getchar();
};

constexpr KernelIO kio;

static_assert(basecpp::IOTrait<KernelIO>, "KernelIO does not satisfy IOTrait");


// Loggers
#include <basecpp/logger.h>

// 通用Logger
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, LOGGER);

// 内存管理相关Logger
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, MEMORY)
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, PAGING);
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, PMM);
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, BUDDY)

// 设备相关Logger
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, DEVICE)
DECLARE_LOGGER(KernelIO, LogLevel::DEBUG, INTERRUPT);