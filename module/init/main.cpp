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

extern "C" {
int kputs(const char *str);
void cpu_idle();
void create_process(const char *path);
}

int kmod_main() {
    kputs("Here is init module!\n");
    create_process("/initrd/default.mod");
    kputs("It shouldn't be printed!\n");
    cpu_idle();
    return 0;
}
