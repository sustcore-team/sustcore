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

extern "C" {
void kwrites(const char *str, size_t len);
size_t sys_grow_vma(size_t heap_base, size_t newbrk);
CapIdx sys_create_process(const char *path, CapIdx *caps, size_t caps_sz);
CapIdx create_process(const char *path, CapIdx *caps, size_t caps_sz);

bool sys_cap_clone(CapIdx src, CapIdx target);
bool sys_cap_downgrade(CapIdx idx, uint64_t new_perm);
bool sys_cap_derive(CapIdx src, CapIdx target, uint64_t new_perm);
bool lookup_cap(CapIdx idx, CapInfo *info);
size_t sys_getpid(CapIdx pcb_cap);

bool sys_create_notification(CapIdx target);
bool sys_signal_notification(CapIdx capidx, size_t idx);
bool sys_unsignal_notification(CapIdx capidx, size_t idx);
bool sys_check_notification(CapIdx capidx, size_t idx);
bool sys_wait_notification(CapIdx capidx, size_t idx);
}

extern "C" {
int kputs(const char *str);
size_t brk(size_t newbrk);
void *sbrk(ptrdiff_t increment);
}
