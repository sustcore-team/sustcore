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

constexpr CapIdx kForkDoneCap    = cap::make(1, 3);
constexpr size_t kForkDoneSignal = 0;

int kmod_main() {
    printf("进入 init 模块!\n");
    if (!sys_notif_create(kForkDoneCap)) {
        printf("init: 创建fork完成通知失败\n");
        while (true) {
        }
    }

    CapIdx initial_caps[] = {kForkDoneCap};
    CapIdx modidx         = sys_create_process("/initrd/test_fork.mod",
                                               (CapIdx *)initial_caps, 1, 3);
    printf("移除test_fork模块能力 %p\n", modidx);
    // don't hold its capability index.
    sys_cap_remove(modidx);

    sys_notif_wait(kForkDoneCap, kForkDoneSignal);
    sys_notif_unsignal(kForkDoneCap, kForkDoneSignal);
    printf("init: 收到test_fork完成通知\n");

    modidx = sys_create_process("/initrd/test_thread.mod", nullptr, 0, 3);
    printf("移除test_thread模块能力 %p\n", modidx);
    sys_cap_remove(modidx);

    modidx =
        sys_create_process("/initrd/test_endpoint_master.mod", nullptr, 0, 3);
    printf("移除test-endpoint-master模块能力 %p\n", modidx);
    sys_cap_remove(modidx);

    modidx = sys_create_process("/initrd/test_call_service.mod", nullptr, 0, 3);
    printf("移除test_call_service模块能力 %p\n", modidx);
    sys_cap_remove(modidx);

    printf("进入 cpu_idle()!\n");
    cpu_idle();
    return 0;
}
