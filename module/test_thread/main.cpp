#include <kmod/syscall.h>

#include <cstddef>
#include <cstdio>

constexpr size_t kSignalA        = 0;
constexpr size_t kSignalB        = 1;
constexpr size_t kSignalDone     = 2;
constexpr size_t kSignalReady    = 3;
constexpr size_t kStackSize      = 16 * 1024;

static volatile size_t current_start = 0;
static volatile size_t x             = 0;
static volatile size_t rounds        = 0;
static volatile bool done            = false;
static volatile bool shutdown        = false;
static volatile bool failed          = false;
static volatile size_t ready_threads = 0;
static CapIdx thread_notif_cap = cap::null;

constexpr size_t TEST_NUMBER_UPPER = 800;
constexpr size_t PROGRESS_GAP      = 1000;
constexpr size_t NUMBER_GAP        = 100;

static void init_thread_gp() {
#if defined(__ARCH_riscv64__)
    asm volatile("la gp, __global_pointer$" ::: "gp");
#endif
}

static void finish_once(const char *who) {
    if (!done) {
        done = true;
        if (current_start % NUMBER_GAP == 0) {
            printf("test_thread: %s finished start=%u x=%u rounds=%u\n", who,
                   current_start, x, rounds);
        }
        sys_notif_signal(thread_notif_cap, kSignalDone);
    }
}

static void thread_a() {
    init_thread_gp();
    ready_threads = ready_threads + 1;
    sys_notif_signal(thread_notif_cap, kSignalReady);
    while (true) {
        sys_notif_wait(thread_notif_cap, kSignalA);
        sys_notif_unsignal(thread_notif_cap, kSignalA);
        if (shutdown) {
            return;
        }
        if (done || failed) {
            continue;
        }

        x      = x * 3 + 1;
        rounds = rounds + 1;
        if (x == 1) {
            finish_once("A");
            continue;
        }
        sys_notif_signal(thread_notif_cap, kSignalB);
    }
}

static void thread_b() {
    init_thread_gp();
    ready_threads = ready_threads + 1;
    sys_notif_signal(thread_notif_cap, kSignalReady);
    while (true) {
        sys_notif_wait(thread_notif_cap, kSignalB);
        sys_notif_unsignal(thread_notif_cap, kSignalB);
        if (shutdown) {
            return;
        }
        if (done || failed) {
            continue;
        }

        while ((x % 2) == 0 && x != 1) {
            x      /= 2;
            rounds  = rounds + 1;
        }

        if (x == 1) {
            finish_once("B");
            continue;
        }
        sys_notif_signal(thread_notif_cap, kSignalA);
    }
}

static void *alloc_stack() {
    void *stack = sbrk(kStackSize);
    if (stack == reinterpret_cast<void *>(-1)) {
        printf("test_thread: 分配线程栈失败!\n");
        exit(-1);
    }
    return stack;
}

static void run_collatz(size_t start) {
    current_start = start;
    x             = start;
    rounds        = 0;
    done          = false;

    if (start == 1) {
        finish_once("main");
        return;
    }

    sys_notif_signal(thread_notif_cap, (start % 2) == 0 ? kSignalB : kSignalA);
    sys_notif_wait(thread_notif_cap, kSignalDone);
    sys_notif_unsignal(thread_notif_cap, kSignalDone);
}

int kmod_main() {
    printf("test_thread: start pid=%u\n", sys_getpid(__pcb_cap));
    thread_notif_cap = sys_notif_create();
    if (thread_notif_cap == cap::error) {
        printf("test_thread: 创建通知失败!\n");
        exit(-1);
    }

    void *stack_a = alloc_stack();
    void *stack_b = alloc_stack();

    CapIdx tcb_a = sys_create_thread(thread_a, stack_a, kStackSize);
    CapIdx tcb_b = sys_create_thread(thread_b, stack_b, kStackSize);
    printf("test_thread: created A=%p B=%p\n", (void *)tcb_a, (void *)tcb_b);
    if (tcb_a == cap::error || tcb_b == cap::error) {
        printf("test_thread: 创建线程失败!\n");
        exit(-1);
    }

    while (ready_threads < 2) {
        sys_notif_wait(thread_notif_cap, kSignalReady);
        sys_notif_unsignal(thread_notif_cap, kSignalReady);
    }

    for (size_t start = 2; start <= TEST_NUMBER_UPPER; ++start) {
        run_collatz(start);
        if (x != 1) {
            failed = true;
            printf("test_thread: 验证失败 start=%u x=%u rounds=%u\n", start, x,
                   rounds);
            shutdown = true;
            sys_notif_signal(thread_notif_cap, kSignalA);
            sys_notif_signal(thread_notif_cap, kSignalB);
            exit(-1);
        }
        if (start % PROGRESS_GAP == 0 || start == TEST_NUMBER_UPPER) {
            printf("test_thread: verified up to %u\n", start);
        }
    }

    shutdown = true;
    sys_notif_signal(thread_notif_cap, kSignalA);
    sys_notif_signal(thread_notif_cap, kSignalB);
    printf("test_thread: verified [2, %u]\n", TEST_NUMBER_UPPER);
    exit(0);
    return 0;
}
