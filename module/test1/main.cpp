#include <cstddef>
#include <cstdio>
#include <kmod/syscall.h>
#include <sustcore/capability.h>

constexpr CapIdx kNotifCap = cap::make(0, 3);

constexpr size_t kSignalSyn = 0;
constexpr size_t kSignalAck = 1;
constexpr size_t kHandshakeRounds = 10;

int kmod_main() {
    printf("test1: start\n");
    printf("test1: pid=%u\n", sys_getpid(__pcb_cap));

    if (!sys_create_notification(kNotifCap)) {
        printf("test1: create notification failed\n");
        while (true) {}
    }

    CapIdx initial_caps[] = {kNotifCap};
    CapIdx test2_pcb =
        sys_create_process("/initrd/test2.mod", (CapIdx *)initial_caps, 1, 3);
    if (test2_pcb == cap::error) {
        printf("test1: create test2 failed\n");
        while (true) {}
    }

    for (size_t round = 0; round < kHandshakeRounds; ++round) {
        printf("test1: round=%d SYN\n", round);
        sys_signal_notification(kNotifCap, kSignalSyn);

        sys_wait_notification(kNotifCap, kSignalAck);
        printf("test1: round=%d ACK\n", round);
        sys_unsignal_notification(kNotifCap, kSignalAck);
    }

    while (true) {}
    return 0;
}
