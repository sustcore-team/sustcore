/**
 * @file fdtable.h
 * @brief Linux-style fd table mapping int fd to kernel CapIdx
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

#include <sustcore/capability.h>

#if defined(__ARCH_riscv64__)
#include <arch/riscv64/spinlock.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/spinlock.h>
#else
#error "Unsupported architecture for spinlock"
#endif

constexpr int MAX_FDS = 1024;
constexpr int CWD_FD_RESERVED = 3;

struct FdEntry {
    CapIdx cap;
    size_t offset;
};

class FdTableLock {
    raw_spinlock_t _lock = 0;
public:
    void lock()   { __raw_spin_lock(&_lock); }
    void unlock() { __raw_spin_unlock(&_lock); }
};

// fd table operations (protected by FdTableLock)
int         alloc_fd(CapIdx cap);
bool        bind_fd(int fd, CapIdx cap);
void        free_fd(int fd);
FdEntry*    lookup_fd(int fd);       // returns nullptr if out of bounds or cap==null
CapIdx      fd_to_cap(int fd);        // returns cap::error on invalid
size_t      fd_offset(int fd);        // returns 0 on invalid
void        set_fd_offset(int fd, size_t offset);
