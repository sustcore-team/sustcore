#include <kmod/syscall.h>
#include <sustcore/capability.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

constexpr CapIdx kCallEndpointCap = cap::make(1, 4);
constexpr uint64_t kOpEncrypt     = 1;
constexpr uint64_t kOpDecrypt     = 2;

struct CallRequest {
    uint64_t op;
    uint64_t key;
    uint64_t value;
};

static void serve_one(CapIdx endpoint) {
    CallRequest request{};
    size_t msgsz = 0;
    CapIdx caps[MAX_MSG_CAPS]{};
    size_t capsz = 0;
    MsgPacket packet{
        .msgbuf  = &request,
        .msgsz   = &msgsz,
        .caplist = caps,
        .capsz   = &capsz,
    };

    sys_endpoint_recv(endpoint, &packet);
    if (msgsz != sizeof(request) || capsz != 1) {
        printf("test_call_service: 请求格式错误 msgsz=%u capsz=%u\n", msgsz,
               capsz);
        while (true) {
        }
    }

    CapInfo reply_info{};
    if (!sys_cap_lookup(caps[0], &reply_info) ||
        reply_info.type != PayloadType::REPLY)
    {
        printf("test_call_service: 未收到reply capability\n");
        while (true) {
        }
    }

    uint64_t result = 0;
    if (request.op == kOpEncrypt) {
        result = request.key ^ request.value;
        printf("test_call_service: encrypt K=0x%016lx V=0x%016lx C=0x%016lx\n",
               request.key, request.value, result);
    } else if (request.op == kOpDecrypt) {
        result = request.key ^ request.value;
        printf("test_call_service: decrypt K=0x%016lx C=0x%016lx V=0x%016lx\n",
               request.key, request.value, result);
    } else {
        printf("test_call_service: 未知操作=%u\n", request.op);
        while (true) {
        }
    }

    size_t reply_msgsz = sizeof(result);
    size_t reply_capsz = 0;
    MsgPacket reply{
        .msgbuf  = &result,
        .msgsz   = &reply_msgsz,
        .caplist = nullptr,
        .capsz   = &reply_capsz,
    };
    endpoint_reply(caps[0], &reply);
}

int kmod_main() {
    printf("test_call_service: start pid=%u\n", sys_getpid(__pcb_cap));
    if (!sys_endpoint_create(kCallEndpointCap)) {
        printf("test_call_service: 创建endpoint失败\n");
        while (true) {
        }
    }

    CapIdx initial_caps[] = {kCallEndpointCap};
    CapIdx user_pcb       = sys_create_process("/initrd/test_call_user.mod",
                                               initial_caps, 1, SCHED_CLASS_RR);
    if (user_pcb == cap::error) {
        printf("test_call_service: 创建user失败\n");
        while (true) {
        }
    }
    sys_cap_remove(user_pcb);

    serve_one(kCallEndpointCap);
    serve_one(kCallEndpointCap);

    printf("test_call_service: done\n");
    while (true) {
    }
    return 0;
}
