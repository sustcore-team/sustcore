#include <kmod/syscall.h>

#include <cstddef>
#include <cstdio>

constexpr CapIdx kThreadNotifCap = cap::make(1, 3);
constexpr size_t kSignalA        = 0;
constexpr size_t kSignalB        = 1;
constexpr size_t kSignalDone     = 2;
constexpr size_t kStackSize      = 16 * 1024;

static volatile size_t x      = 27;
static volatile size_t rounds = 0;
static volatile bool done     = false;

static void finish_once(const char *who) {
    if (!done) {
        done = true;
        printf("test_thread: %s done x=%u rounds=%u\n", who, x, rounds);
        sys_notif_signal(kThreadNotifCap, kSignalDone);
    }
}

static void thread_a() {
    while (true) {
        sys_notif_wait(kThreadNotifCap, kSignalA);
        sys_notif_unsignal(kThreadNotifCap, kSignalA);
        if (done) {
            continue;
        }

        x      = x * 3 + 1;
        rounds = rounds + 1;
        printf("test_thread: A x=%u rounds=%u\n", x, rounds);

        if (x == 1) {
            finish_once("A");
            continue;
        }
        sys_notif_signal(kThreadNotifCap, kSignalB);
    }
}

static void thread_b() {
    while (true) {
        sys_notif_wait(kThreadNotifCap, kSignalB);
        sys_notif_unsignal(kThreadNotifCap, kSignalB);
        if (done) {
            continue;
        }

        while ((x % 2) == 0 && x != 1) {
            x      /= 2;
            rounds  = rounds + 1;
            printf("test_thread: B x=%u rounds=%u\n", x, rounds);
        }

        if (x == 1) {
            finish_once("B");
            continue;
        }
        sys_notif_signal(kThreadNotifCap, kSignalA);
    }
}

static void *alloc_stack() {
    void *stack = sbrk(kStackSize);
    if (stack == reinterpret_cast<void *>(-1)) {
        printf("test_thread: 分配线程栈失败!\n");
        while (true) {
        }
    }
    return stack;
}

int kmod_main() {
    printf("test_thread: start pid=%u\n", sys_getpid(__pcb_cap));
    if (!sys_notif_create(kThreadNotifCap)) {
        printf("test_thread: 创建通知失败!\n");
        while (true) {
        }
    }

    void *stack_a = alloc_stack();
    void *stack_b = alloc_stack();

    CapIdx tcb_a = sys_create_thread(thread_a, stack_a, kStackSize);
    CapIdx tcb_b = sys_create_thread(thread_b, stack_b, kStackSize);
    printf("test_thread: created A=%p B=%p\n", (void *)tcb_a, (void *)tcb_b);
    if (tcb_a == cap::error || tcb_b == cap::error) {
        printf("test_thread: 创建线程失败!\n");
        while (true) {
        }
    }

    sys_notif_signal(kThreadNotifCap, kSignalA);
    sys_notif_wait(kThreadNotifCap, kSignalDone);
    sys_notif_unsignal(kThreadNotifCap, kSignalDone);

    printf("test_thread: final x=%u rounds=%u\n", x, rounds);
    sys_exit();
    while (true) {
    }
    return 0;
}
