/**
 * @file prog.h
 * @author theflysong
 * @brief linux subsystem 中用户程序相关运行时数据声明
 * @version alpha-1.0.0
 * @date 2026-06-22
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

#include <sustcore/bootstrap.h>
#include <sustcore/capability.h>

extern size_t __prog_heap_base;
extern size_t __prog_brk;
extern CapIdx __prog_pcb_cap;
extern CapIdx __prog_main_tcb_cap;
extern CapIdx __prog_heap_mem_cap;
extern CapIdx __prog_root_dir_cap;

void init_prog_data(size_t bsargc, const bsheader *bsargv[]);
size_t linux_sys_brk(size_t newbrk);
[[noreturn]]
void linux_sys_exit(int exitcode);
size_t linux_sys_uname(void *buf);
size_t linux_sys_wait4(int pid, int *status, int options, void *rusage);
size_t linux_sys_clone(size_t flags, addr_t newsp, int *parent_tid,
                       int *child_tid, addr_t tls,
                       addr_t dispatch_frame_sp);
// VFS syscall handlers
size_t linux_sys_openat(int dirfd, const char *pathname, int flags, int mode);
size_t linux_sys_close(int fd);
size_t linux_sys_read(int fd, void *buf, size_t count);
size_t linux_sys_lseek(int fd, size_t offset, int whence);

extern "C" [[noreturn]] void linux_clone_fork_return_trampoline(
    addr_t newsp, addr_t dispatch_frame_sp);
