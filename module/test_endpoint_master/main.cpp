#include <kmod/syscall.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr CapIdx kEndpointCap = cap::make(1, 3);
constexpr uint64_t kValueV    = 0x123456789abcdef0ULL;
constexpr size_t kRepeatCount = 10;

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
    printf("test-endpoint-master: start\n");
    printf("test-endpoint-master: pid=%u\n", sys_getpid(__pcb_cap));

    if (!sys_endpoint_create(kEndpointCap)) {
        printf("test-endpoint-master: 创建端点失败!\n");
        sys_exit();
    }

    CapIdx initial_caps[] = {kEndpointCap};
    CapIdx slave_pcb = sys_create_process("/initrd/test_endpoint_slave.mod",
                                          (CapIdx *)initial_caps, 1, 3);
    if (slave_pcb == cap::error) {
        printf("test-endpoint-master: 创建 test-endpoint-slave 失败!\n");
        sys_exit();
    }

    uint64_t k = recv_u64(kEndpointCap, "test-endpoint-master");
    printf("test-endpoint-master: 收到密钥 K=0x%016lx\n", k);

    for (size_t round = 0; round < kRepeatCount; ++round) {
        uint64_t v   = kValueV + round;
        uint64_t c   = k ^ v;
        size_t msgsz = sizeof(c);
        size_t capsz = 0;
        MsgPacket packet{
            .msgbuf  = &c,
            .msgsz   = &msgsz,
            .caplist = nullptr,
            .capsz   = &capsz,
        };
        printf("test-endpoint-master: 第%u轮 V=0x%016lx C=0x%016lx\n", round, v,
               c);
        sys_endpoint_send(kEndpointCap, &packet);
    }

    sys_exit();
    return 0;
}
