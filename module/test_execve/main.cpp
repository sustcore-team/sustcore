#include <kmod/syscall.h>

#include <cstddef>
#include <cstdio>

constexpr size_t kSignalSyn    = 0;
constexpr size_t kSignalSynAck = 1;
constexpr size_t kSignalAck    = 2;
constexpr size_t kScanGroups   = 2;
constexpr size_t kScanSlots    = 32;

static CapIdx find_unique_notif_cap() {
    CapIdx found = cap::null;
    size_t count = 0;

    for (size_t group = 0; group < kScanGroups; ++group) {
        for (size_t slot = 0; slot < kScanSlots; ++slot) {
            CapIdx candidate = cap::make(group, slot);
            CapInfo info{};
            if (!sys_cap_lookup(candidate, &info) ||
                info.type != PayloadType::NOTIF)
            {
                continue;
            }
            found = candidate;
            ++count;
        }
    }

    if (count != 1) {
        printf(
            "test_execve: 预期找到一个 notification capability, 但是找到了 %u "
            "个\n",
            count);
        while (true) {
        }
    }
    return found;
}

int kmod_main() {
    printf("test_execve: pid=%u pcb_cap=%p\n", sys_getpid(__pcb_cap),
           (void *)__pcb_cap);

    CapIdx notif_cap = find_unique_notif_cap();
    printf("test_execve: notification cap=%p\n", (void *)notif_cap);

    sys_notif_wait(notif_cap, kSignalSyn);
    printf("test_execve: 接收 SYN\n");
    sys_notif_unsignal(notif_cap, kSignalSyn);

    sys_notif_signal(notif_cap, kSignalSynAck);
    printf("test_execve: 发送 SYN-ACK\n");

    sys_notif_wait(notif_cap, kSignalAck);
    printf("test_execve: 接收 ACK\n");
    sys_notif_unsignal(notif_cap, kSignalAck);

    printf("test_execve: 握手完成!\n");
    sys_exit();
    while (true) {
    }
    return 0;
}
