#include "fdtable.h"

#include <errno.h>

#include <kmod/syscall.h>

static FdEntry fd_table[MAX_FDS] = {{cap::null, 0}};
static FdTableLock fd_table_lock;

int alloc_fd(CapIdx cap) {
    fd_table_lock.lock();

    int fd = -EMFILE;
    for (int i = CWD_FD_RESERVED + 1; i < MAX_FDS; ++i) {
        if (fd_table[i].cap == cap::null) {
            fd_table[i].cap    = cap;
            fd_table[i].offset = 0;
            fd                 = i;
            break;
        }
    }

    fd_table_lock.unlock();
    return fd;
}

bool bind_fd(int fd, CapIdx cap) {
    if (fd < 0 || fd >= MAX_FDS || cap == cap::null || cap == cap::error) {
        return false;
    }

    fd_table_lock.lock();

    CapIdx old_cap = fd_table[fd].cap;
    if (old_cap != cap::null && old_cap != cap) {
        sys_cap_remove(old_cap);
    }
    fd_table[fd].cap    = cap;
    fd_table[fd].offset = 0;

    fd_table_lock.unlock();
    return true;
}

void free_fd(int fd) {
    if (fd < 0 || fd >= MAX_FDS) {
        return;
    }

    fd_table_lock.lock();

    CapIdx cap = fd_table[fd].cap;
    if (cap != cap::null) {
        sys_cap_remove(cap);
        fd_table[fd] = {cap::null, 0};
    }

    fd_table_lock.unlock();
}

FdEntry *lookup_fd(int fd) {
    if (fd < 0 || fd >= MAX_FDS) {
        return nullptr;
    }
    if (fd_table[fd].cap == cap::null) {
        return nullptr;
    }
    return &fd_table[fd];
}

CapIdx fd_to_cap(int fd) {
    if (fd < 0 || fd >= MAX_FDS) {
        return cap::error;
    }
    CapIdx c = fd_table[fd].cap;
    if (c == cap::null) {
        return cap::error;
    }
    return c;
}

size_t fd_offset(int fd) {
    if (fd < 0 || fd >= MAX_FDS) {
        return 0;
    }
    if (fd_table[fd].cap == cap::null) {
        return 0;
    }
    return fd_table[fd].offset;
}

void set_fd_offset(int fd, size_t offset) {
    if (fd < 0 || fd >= MAX_FDS) {
        return;
    }
    if (fd_table[fd].cap == cap::null) {
        return;
    }
    fd_table[fd].offset = offset;
}
