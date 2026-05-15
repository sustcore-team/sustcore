/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 主文件
 * @version alpha-1.0.0
 * @date 2026-04-28
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cstddef>
#include <sustcore/capability.h>

extern "C" {
int kputs(const char *str);
void cpu_idle();
CapIdx sys_create_process(const char *path, CapIdx *caps, size_t caps_sz);
}

int kmod_main() {
    kputs("Here is init module!\n");
    sys_create_process("/initrd/test1.mod", nullptr, 0);
    kputs("It shouldn't be printed!\n");
    cpu_idle();
    return 0;
}
