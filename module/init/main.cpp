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

constexpr CapIdx kForkDoneCap = cap::make(0, 3);
constexpr size_t kForkDoneSignal = 0;

int kmod_main() {
    printf("Here is init module!\n");
    if (!sys_create_notification(kForkDoneCap)) {
        printf("init: create fork completion notification failed\n");
        while (true) {
        }
    }

    CapIdx initial_caps[] = {kForkDoneCap};
    CapIdx modidx = sys_create_process("/initrd/test_fork.mod", (CapIdx *)initial_caps, 1, 3);
    printf("remove test_fork module capability %p\n", modidx);
    // don't hold its capability index.
    sys_cap_remove(modidx);

    sys_wait_notification(kForkDoneCap, kForkDoneSignal);
    sys_unsignal_notification(kForkDoneCap, kForkDoneSignal);
    printf("init: test_fork completion received\n");

    modidx = sys_create_process("/initrd/test_thread.mod", nullptr, 0, 3);
    printf("remove test_thread module capability %p\n", modidx);
    sys_cap_remove(modidx);

    printf("进入 cpu_idle()!\n");
    cpu_idle();
    return 0;
}
