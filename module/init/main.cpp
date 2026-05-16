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

#include <kmod/syscall.h>
#include <cstdio>

extern "C" {
void cpu_idle();
}

int kmod_main() {
    printf("Here is init module!\n");
    CapIdx modidx = sys_create_process("/initrd/test_fork.mod", nullptr, 0);
    printf("remove module capability %zu\n", modidx);
    // don't hold its capability index.
    sys_cap_remove(modidx);
    printf("进入 cpu_idle()!\n");
    cpu_idle();
    return 0;
}
