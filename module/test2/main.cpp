#include <cstddef>
#include <cstdio>
#include <kmod/syscall.h>

constexpr size_t kSignalSyn = 0;
constexpr size_t kSignalAck = 1;
constexpr size_t kHandshakeRounds = 10;
constexpr size_t kScanSlots = 16;

// This test intentionally discovers the injected notification capability by
// metadata instead of depending on a fixed CSpace slot chosen by the creator.
static CapIdx find_unique_notification_cap() {
    CapIdx found = cap::null;
    size_t count = 0;

    for (size_t slot = 0; slot < kScanSlots; ++slot) {
        CapIdx candidate = cap::make(0, slot);
        CapInfo info{};
        if (!lookup_cap(candidate, &info) || info.type != PayloadType::NOTIF) {
            continue;
        }

        found = candidate;
        ++count;
    }

    if (count != 1) {
        printf("test2: expected one notification cap, found %u\n", count);
        while (true) {}
    }

    return found;
}

int kmod_main() {
    printf("test2: start\n");
    printf("test2: pid=%u\n", sys_getpid(__pcb_cap));

    CapIdx notif_cap = find_unique_notification_cap();

    for (size_t round = 0; round < kHandshakeRounds; ++round) {
        sys_wait_notification(notif_cap, kSignalSyn);
        printf("test2: round=%d SYN-ACK\n", round);
        sys_unsignal_notification(notif_cap, kSignalSyn);
        sys_signal_notification(notif_cap, kSignalAck);
    }

    while (true) {}
    return 0;
}
