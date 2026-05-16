#include <kmod/syscall.h>

#include <cstddef>
#include <cstdio>
#include <cstring>

static volatile size_t global_value = 0;

constexpr size_t kScanGroups = 1;
constexpr size_t kScanSlots  = 32;

static const char *cap_type_name(PayloadType type) {
    return to_string(type);
}

static void dump_caps(const char *tag) {
    printf("%s: capability dump begin\n", tag);
    for (size_t group = 0; group < kScanGroups; ++group) {
        for (size_t slot = 0; slot < kScanSlots; ++slot) {
            CapIdx idx = cap::make(group, slot);
            CapInfo info{};
            if (!lookup_cap(idx, &info)) {
                printf("%s: 编号=%p empty\n", tag, (void *)idx);
                continue;
            }
            printf("%s: 编号=%p 类型=%s 权限=%p\n", tag, (void *)idx,
                   cap_type_name(info.type), (void *)info.permissions);
        }
    }
    printf("%s: capability dump end\n", tag);
}

static char *alloc_page_string(const char *str) {
    char *buf = static_cast<char *>(sbrk(4096));
    if (buf == reinterpret_cast<char *>(-1)) {
        printf("test_fork: sbrk failed\n");
        while (true) {
        }
    }
    strcpy(buf, str);
    return buf;
}

int kmod_main() {
    printf("test_fork: 启动时PID=%u pcb_cap=%p\n", sys_getpid(__pcb_cap),
           (void *)__pcb_cap);

    global_value     = 114514;
    char *shared_buf = alloc_page_string("全体目光向我看齐");

    ForkRet ret = sys_fork();
    if (ret.ret1 == cap::error) {
        printf("test_fork: fork failed\n");
        while (true) {
        }
    }

    bool is_child        = ret.ret2 == 0;
    const char *tag      = is_child ? "child" : "parent";
    size_t abi_pcb_pid   = sys_getpid(__pcb_cap);
    size_t child_cap_pid = sys_getpid(ret.ret1);
    printf(
        "test_fork: %s fork后 子进程capidx=%p 子进程pid=%u ABI获得的PCB PID=%u "
        "子进程PID=%u global=%u shared=%s\n",
        tag, (void *)ret.ret1, ret.ret2, abi_pcb_pid, child_cap_pid,
        global_value, shared_buf);

    if (is_child) {
        global_value = 1919;
        strcpy(shared_buf, "看我看我");
    } else {
        global_value = 810;
        strcpy(shared_buf, "我宣布个事");
    }

    printf("test_fork: %s COW写后 global=%u shared=%s\n", tag, global_value,
           shared_buf);

    char *private_buf = alloc_page_string("ABC");
    if (is_child) {
        strcpy(private_buf, "XYZ");
    } else {
        strcpy(private_buf, "UVW");
    }
    printf("test_fork: %s private buf=%s\n", tag, private_buf);

    dump_caps(is_child ? "子进程" : "父进程");
    printf("test_fork: %s exit\n", tag);
    sys_exit();

    while (true) {
    }
    return 0;
}
