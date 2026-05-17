#include <kmod/syscall.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr uint64_t kValueK    = 0xfedcba9876543210ULL;
constexpr size_t kRepeatCount = 10;
constexpr size_t kScanSlots   = 16;

// test-endpoint-slave 模块: 从 test-endpoint-master 接收一个值,
// 进行异或运算后再发回去, 重复多轮 这里寻找能力空间中唯一的端点能力, 从而与
// test-endpoint-master 进行通信
static CapIdx find_unique_endpoint_cap() {
    CapIdx found = cap::null;
    size_t count = 0;

    for (size_t slot = 0; slot < kScanSlots; ++slot) {
        CapIdx candidate = cap::make(1, slot);
        CapInfo info{};
        if (!sys_cap_lookup(candidate, &info) ||
            info.type != PayloadType::ENDPOINT)
        {
            continue;
        }

        found = candidate;
        ++count;
    }

    if (count != 1) {
        printf(
            "test-endpoint-slave: 预期找到一个端点 capability, 但是找到了 %u "
            "个\n",
            count);
        sys_exit();
    }

    return found;
}

static uint64_t recv_u64(CapIdx endpoint, const char *tag) {
    uint64_t value = 0;
    size_t msgsz   = 0;
    size_t capsz   = 0;
    MsgPacket packet{
        .msgbuf  = &value,
        .msgsz   = &msgsz,
        .caplist = nullptr,
        .capsz   = &capsz,
    };

    sys_endpoint_recv(endpoint, &packet);
    if (msgsz != sizeof(value) || capsz != 0) {
        printf("%s: 无效的消息 msgsz=%u capsz=%u\n", tag, msgsz, capsz);
        sys_exit();
    }

    return value;
}

int kmod_main() {
    printf("test-endpoint-slave 启动! \n");
    printf("test-endpoint-slave: pid=%u\n", sys_getpid(__pcb_cap));

    CapIdx endpoint_cap = find_unique_endpoint_cap();

    printf("test-endpoint-slave: 发送密钥 K=0x%016lx\n", kValueK);
    uint64_t value = kValueK;
    size_t msgsz   = sizeof(kValueK);
    size_t capsz   = 0;
    MsgPacket packet{
        .msgbuf  = &value,
        .msgsz   = &msgsz,
        .caplist = nullptr,
        .capsz   = &capsz,
    };
    sys_endpoint_send(endpoint_cap, &packet);

    for (size_t round = 0; round < kRepeatCount; ++round) {
        uint64_t c = recv_u64(endpoint_cap, "test-endpoint-slave");
        uint64_t v = c ^ kValueK;
        printf("test-endpoint-slave: 第%u轮 K=0x%016lx C=0x%016lx V=0x%016lx\n",
               round, kValueK, c, v);
    }

    sys_exit();
    return 0;
}
