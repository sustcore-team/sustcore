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
    int putchar(char c);
    int puts(const char* str);
    char getchar();
};

static_assert(basecpp::IOTrait<KernelIO>, "KernelIO does not satisfy IOTrait");

extern KernelIO kio;