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
#include <string>
#include <vector>

#include <sustcore/bootstrap.h>
#include <sustcore/capability.h>

constexpr size_t LINUX_PATH_MAX = 256;
constexpr int CWD_FD            = 3;

extern size_t __prog_heap_base;
extern size_t __prog_brk;
extern CapIdx __prog_pcb_cap;
extern CapIdx __prog_parent_cap;
extern CapIdx __prog_main_tcb_cap;
extern CapIdx __prog_heap_mem_cap;
extern CapIdx __prog_root_dir_cap;
extern CapIdx __prog_cwd_dir_cap;
extern std::vector<CapIdx> __prog_children;
extern std::string __prog_cwd;
extern std::string __prog_image_path;

void init_prog_data(size_t argc, const char *argv[], size_t bsargc,
                    const bsheader *bsargv[]);
size_t linux_sys_brk(size_t newbrk);
[[noreturn]]
void linux_sys_exit(int exitcode);
[[noreturn]]
void linux_sys_exit_group(int exitcode);
size_t linux_sys_getpid();
size_t linux_sys_getppid();
size_t linux_sys_sched_yield();
size_t linux_sys_chdir(const char *pathname);
size_t linux_sys_uname(void *buf);
size_t linux_sys_gettimeofday(void *tv, void *tz);
size_t linux_sys_nanosleep(const void *req, void *rem);
size_t linux_sys_times(void *buf);
size_t linux_sys_getrandom(void *buf, size_t buflen, unsigned flags);
size_t linux_sys_execve(const char *pathname, const char *const argv[],
                        const char *const envp[]);
size_t linux_sys_wait4(int pid, int *status, int options, void *rusage);
size_t linux_sys_clone(size_t flags, addr_t newsp, int *parent_tid,
                       int *child_tid, addr_t tls,
                       addr_t dispatch_frame_sp);

extern "C" [[noreturn]] void linux_clone_fork_return_trampoline(
    addr_t newsp, addr_t dispatch_frame_sp);
