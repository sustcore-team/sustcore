/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief the main file for usrboot
 * @version 0.1.0-dev.1
 * @date 2026-08-11
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <log.h>
#include <usrboot_syscall.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    logger::info("usrboot logger 与 yield syscall 已就绪");
    while (true) usrboot_yield();
    return 0;
}
