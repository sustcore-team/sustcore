/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief kmod系统调用接口
 * @version alpha-1.0.0
 * @date 2026-05-14
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <cstddef>
#include <cstdint>
#include <sustcore/capability.h>

extern CapIdx __pcb_cap;
extern CapIdx __main_tcb_cap;
extern CapIdx __heap_mem_cap;
extern CapIdx __stack_mem_cap;

enum KmodSchedClass : size_t {
    SCHED_CLASS_FCFS = 2,
    SCHED_CLASS_RR   = 3,
};

struct ForkRet {
    CapIdx ret1;
    size_t ret2;
};

extern "C" {
void sys_write_serial(const char *str, size_t len);
void sys_exit();
bool sys_pcb_kill(CapIdx pcb_cap, int exit_code);
bool sys_pcb_map(CapIdx pcb_cap, CapIdx mem_cap, void *vaddr, uint64_t rwx,
                 uint64_t growth);
CapIdx sys_pcb_create_process(CapIdx pcb_cap, const char *path, CapIdx *caps,
                              size_t caps_sz, size_t sched_class);
CapIdx sys_create_process(const char *path, CapIdx *caps, size_t caps_sz,
                          size_t sched_class);
CapIdx sys_pcb_create_thread(CapIdx pcb_cap, void (*entry)(),
                             void *stack_addr, size_t stack_size);
CapIdx sys_create_thread(void (*entry)(), void *stack_addr, size_t stack_size);
ForkRet sys_pcb_fork(CapIdx pcb_cap);
ForkRet fork();
bool sys_pcb_execve(CapIdx pcb_cap, const char *path, CapIdx *rsvdlst,
                    size_t rsvdsz);
bool sys_execve(const char *path, CapIdx *rsvdlst, size_t rsvdsz);
bool execve(const char *path, CapIdx *rsvdlst, size_t rsvdsz);

bool sys_cap_clone(CapIdx src, CapIdx target);
bool sys_cap_downgrade(CapIdx idx, uint64_t new_perm);
bool sys_cap_derive(CapIdx src, CapIdx target, uint64_t new_perm);
bool sys_cap_remove(CapIdx idx);
bool sys_cap_lookup(CapIdx idx, CapInfo *info);
size_t sys_getpid(CapIdx pcb_cap);

bool sys_notif_create(CapIdx target);
bool sys_notif_signal(CapIdx capidx, size_t idx);
bool sys_notif_unsignal(CapIdx capidx, size_t idx);
bool sys_notif_check(CapIdx capidx, size_t idx);
bool sys_notif_wait(CapIdx capidx, size_t idx);

bool sys_endpoint_create(CapIdx target);
/**
 * @brief 阻塞地向endpoint发送一条MsgPacket描述的消息.
 */
void sys_endpoint_send(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 阻塞地从endpoint接收一条消息并写回MsgPacket描述的缓冲区.
 */
void sys_endpoint_recv(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 非阻塞地向endpoint发送一条消息.
 */
bool sys_endpoint_send_async(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 非阻塞地从endpoint接收一条消息.
 */
bool sys_endpoint_recv_async(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 发起一次同步endpoint调用, 自动携带一次性Reply Capability.
 *
 * @param endpoint 目标endpoint capability.
 * @param sendmsg 请求消息.
 * @param replymsg 用于接收回复消息的缓冲区描述符.
 */
void endpoint_call(CapIdx endpoint, MsgPacket *sendmsg, MsgPacket *replymsg);
/**
 * @brief 使用Reply Capability回复一次endpoint_call.
 *
 * 成功回复后, reply_cap 会从当前CSpace中移除.
 */
void endpoint_reply(CapIdx reply_cap, MsgPacket *replymsg);

bool sys_mem_create(CapIdx idx, size_t memsz, bool shared, bool continuity,
                    uint64_t growth);
bool sys_mem_map(CapIdx idx, void *vaddr, uint64_t rwx, uint64_t growth);
bool sys_mem_unmap(CapIdx idx, void *vaddr);
bool sys_mem_resize(CapIdx idx, size_t newsz);
ForkRet sys_mem_query(CapIdx idx);
}

extern "C" {
int kputs(const char *str);
size_t brk(size_t newbrk);
void *sbrk(ptrdiff_t increment);
}
