/**
 * @file syscall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief syscall function
 * @version alpha-1.0.0
 * @date 2026-05-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <kmod/syscall.h>
#include <prm.h>

#include <cstring>

extern "C" {
void sys_exit() {
    sys_pcb_kill(__pcb_cap, 0);
    while (true) {}
}

CapIdx sys_create_process(const char *path, CapIdx *caps, size_t caps_sz,
                          size_t sched_class) {
    return sys_pcb_create_process(__pcb_cap, path, caps, caps_sz, sched_class);
}

CapIdx sys_create_thread(void (*entry)(), void *stack_addr,
                         size_t stack_size) {
    return sys_pcb_create_thread(__pcb_cap, entry, stack_addr, stack_size);
}

ForkRet fork() {
    ForkRet ret = sys_pcb_fork(__pcb_cap);
    if (ret.ret1 != cap::error && ret.ret2 == 0) {
        __pcb_cap = ret.ret1;
    }
    return ret;
}

bool sys_execve(const char *path, CapIdx *rsvdlst, size_t rsvdsz) {
    return sys_pcb_execve(__pcb_cap, path, rsvdlst, rsvdsz);
}

bool execve(const char *path, CapIdx *rsvdlst, size_t rsvdsz) {
    return sys_execve(path, rsvdlst, rsvdsz);
}

bool sys_mem_map(CapIdx idx, void *vaddr, uint64_t rwx, uint64_t growth) {
    return sys_pcb_map(__pcb_cap, idx, vaddr, rwx, growth);
}

size_t brk(size_t newbrk) {
    if (newbrk == 0) {
        return __current_brk;
    }

    if (newbrk < __heap_base) {
        return __current_brk;
    }
    if (!sys_mem_resize(__heap_mem_cap, newbrk - __heap_base)) {
        return __current_brk;
    }
    size_t actual_brk = newbrk;
    __current_brk     = actual_brk;
    return __current_brk;
}

void *sbrk(ptrdiff_t increment) {
    size_t old_brk = __current_brk;
    size_t newbrk  = old_brk;

    if (increment >= 0) {
        size_t inc = static_cast<size_t>(increment);
        if (SIZE_MAX - old_brk < inc) {
            return reinterpret_cast<void *>(-1);
        }
        newbrk = old_brk + inc;
    } else {
        size_t dec = size_t(0) - static_cast<size_t>(increment);
        if (old_brk < dec) {
            return reinterpret_cast<void *>(-1);
        }
        newbrk = old_brk - dec;
    }

    size_t actual_brk = brk(newbrk);
    if (actual_brk != newbrk) {
        return reinterpret_cast<void *>(-1);
    }
    return reinterpret_cast<void *>(old_brk);
}
}
