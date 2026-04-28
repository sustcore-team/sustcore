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

void kputs(const char *str);

extern "C" {
void cpu_idle();
}

int kmod_main() {
    kputs("Here is init module!");
    cpu_idle();
    return 0;
}