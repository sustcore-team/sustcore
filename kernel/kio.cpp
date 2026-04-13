/**
 * @file kio.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kernel I/O implementation
 * @version alpha-1.0.0
 * @date 2026-04-13
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <kio.h>
#include <arch/description.h>
#include <sustcore/addr.h>

int kputs(const char *str) {
    size_t len        = strlen(str);
    PhyAddr str_paddr = convert_pointer(str);
    Serial::serial_write_string(len, str_paddr.as<char>());
    return strlen(str);
}

int kputchar(char ch) {
    Serial::serial_write_char(ch);
    return ch;
}

char kgetchar() {
    return '\0';
}

int KernelIO::putchar(char c) {
    return kputchar(c);
}

int KernelIO::puts(const char *str) {
    return kputs(str);
}

char KernelIO::getchar() {
    return kgetchar();
}

int kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf<KernelIO>(fmt, args);
    va_end(args);
    return len;
}

int kprintfln(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf<KernelIO>(fmt, args);
    va_end(args);
    kputchar('\n');
    return len + 1;
}