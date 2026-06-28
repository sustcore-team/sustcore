/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief linux subsystem libc syscall declarations
 * @version alpha-1.0.0
 * @date 2026-06-21
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <cstddef>

#include <sustcore/capability.h>
#include <sustcore/execve.h>
#include <sustcore/files.h>
#include <sustcore/msg.h>
#include <sustcore/sysret.h>

struct MemQueryRet {
    size_t memsz;
    size_t allocated;
};

struct VMAInfo {
    b64 vma_type;
    b64 vma_prot;
    void *vma_start;
    size_t vma_size;
    CapIdx mem_cap;
};

struct PipeCreateRet {
    CapIdx read_cap;
    CapIdx write_cap;
};

extern "C" SysRet<void> sys_write_serial(size_t __always_zero, const char *str,
                                         size_t len);
extern "C" SysRet<size_t> sys_time_now_ns();
extern "C" SysRet<void> sys_tcb_nanosleep(size_t ns);
extern "C" SysRet<size_t> sys_tcb_get_tid(CapIdx tcb_cap);
extern "C" SysRet<void> sys_tcb_kill(CapIdx tcb_cap, int exit_code);
extern "C" SysRet<void> sys_pcb_kill(CapIdx pcb_cap, int exit_code);
extern "C" SysRet<size_t> sys_pcb_query_vaddr(CapIdx pcb_cap, void *vaddr,
                                              VMAInfo *info);
extern "C" SysRet<CapIdx> sys_pcb_create_process(CapIdx pcb_cap,
                                                 size_t sched_class,
                                                 const ExecveRequest *request);
extern "C" SysRet<CapIdx> sys_pcb_create_linux_process(
    CapIdx pcb_cap, size_t sched_class, const ExecveRequest *request);
extern "C" SysRet<CapIdx> sys_pcb_create_thread(CapIdx pcb_cap, void (*entry)(),
                                                void *stack_addr,
                                                size_t stack_size);
extern "C" SysRet<CapIdx> sys_tcb_wait(CapIdx tcb_cap, CapIdx pcbs_idx[],
                                       int *status, size_t options);
extern "C" SysRet<size_t> sys_pcb_fork(CapIdx pcb_cap, CapIdx *child_pcb_cap);
extern "C" SysRet<void> sys_pcb_execve(CapIdx pcb_cap,
                                       const ExecveRequest *request);
extern "C" SysRet<void> sys_pcb_execve_linux(CapIdx pcb_cap,
                                             const ExecveRequest *request);
extern "C" SysRet<CapIdx> sys_pcb_procfs_get(CapIdx pcb_cap, const char *name);
extern "C" SysRet<void> sys_pcb_procfs_redirect(CapIdx pcb_cap,
                                                const char *name,
                                                const char *target);
extern "C" SysRet<size_t> sys_yield();
extern "C" SysRet<size_t> sys_getpid(CapIdx pcb_cap);
extern "C" SysRet<CapIdx> sys_notif_create();
extern "C" SysRet<void> sys_notif_signal(CapIdx capidx, size_t idx);
extern "C" SysRet<void> sys_notif_unsignal(CapIdx capidx, size_t idx);
extern "C" SysRet<bool> sys_notif_check(CapIdx capidx, size_t idx);
extern "C" SysRet<void> sys_notif_wait(CapIdx capidx, size_t idx);
extern "C" SysRet<CapIdx> sys_cap_clone(CapIdx src);
extern "C" SysRet<void> sys_cap_downgrade(CapIdx idx, uint64_t new_perm);
extern "C" SysRet<CapIdx> sys_cap_derive(CapIdx src, uint64_t new_perm);
extern "C" SysRet<void> sys_cap_remove(CapIdx idx);
extern "C" SysRet<void> sys_cap_lookup(CapIdx idx, CapInfo *info);
extern "C" SysRet<CapIdx> sys_endpoint_create();
extern "C" SysRet<void> sys_endpoint_send(CapIdx endpoint, MsgPacket *packet);
extern "C" SysRet<void> sys_endpoint_recv(CapIdx endpoint, MsgPacket *packet);
extern "C" SysRet<bool> sys_endpoint_send_async(CapIdx endpoint, MsgPacket *packet);
extern "C" SysRet<bool> sys_endpoint_recv_async(CapIdx endpoint, MsgPacket *packet);
extern "C" SysRet<void> endpoint_call(CapIdx endpoint, MsgPacket *sendmsg,
                                      MsgPacket *replymsg);
extern "C" SysRet<void> endpoint_reply(CapIdx reply_cap, MsgPacket *replymsg);
extern "C" SysRet<CapIdx> sys_mem_create(CapIdx file_cap, size_t memsz,
                                         bool shared, bool continuity,
                                         uint64_t growth, size_t file_offset);
extern "C" SysRet<void> sys_mem_unmap(CapIdx idx, void *vaddr);
extern "C" SysRet<void> sys_mem_resize(CapIdx idx, size_t newsz);
extern "C" SysRet<void> sys_mem_query(CapIdx idx, MemQueryRet *out);
extern "C" SysRet<void> sys_pcb_map(CapIdx pcb_cap, CapIdx mem_cap, size_t offset,
                                    void *vaddr, size_t sz, uint64_t protflg);
extern "C" SysRet<void> sys_pcb_unmap(CapIdx pcb_cap, void *vaddr, size_t sz);
extern "C" SysRet<size_t> sys_pcb_query_vspace(CapIdx pcb_cap, size_t offset,
                                               VMAInfo *info_array,
                                               size_t max_entries);
extern "C" SysRet<CapIdx> sys_vfs_opendir(CapIdx parent_dir_cap, const char *path,
                                          flags::oflg_t oflags);
extern "C" SysRet<CapIdx> sys_vfs_open(CapIdx parent_dir_cap, const char *path,
                                       flags::oflg_t oflags);
extern "C" SysRet<CapIdx> sys_vfs_mkfile(CapIdx parent_dir_cap, const char *path,
                                         flags::oflg_t oflags);
extern "C" SysRet<CapIdx> sys_vfs_mkdir(CapIdx parent_dir_cap, const char *path,
                                        flags::oflg_t oflags);
extern "C" SysRet<void> sys_vfs_unlink(CapIdx parent_dir_cap, const char *name);
extern "C" SysRet<void> sys_vfs_rmdir(CapIdx parent_dir_cap, const char *name);
extern "C" SysRet<void> sys_vfs_truncate(CapIdx file_cap, size_t new_size);
extern "C" SysRet<void> sys_vfs_rename(CapIdx old_parent_cap, const char *old_name,
                                       CapIdx new_parent_cap, const char *new_name);
extern "C" SysRet<void> sys_vfs_symlink(CapIdx parent_dir_cap, const char *name,
                                        const char *target);
extern "C" SysRet<void> sys_vfs_link(CapIdx parent_dir_cap, const char *name,
                                     CapIdx target);
extern "C" SysRet<void> sys_vfs_stat(CapIdx parent_dir_cap, const char *name,
                                     NodeMeta *out);
extern "C" SysRet<void> sys_vfs_lstat(CapIdx parent_dir_cap, const char *name,
                                      NodeMeta *out);
extern "C" SysRet<void> sys_vfs_fstat(CapIdx file_cap, NodeMeta *out);
extern "C" SysRet<size_t> sys_vfs_readlink(CapIdx parent_dir_cap, const char *name,
                                           char *buf, size_t bufsiz);
extern "C" SysRet<size_t> sys_vfs_read(CapIdx file_cap, size_t offset, void *buf,
                                       size_t len);
extern "C" SysRet<size_t> sys_vfs_write(CapIdx file_cap, size_t offset,
                                        const void *buf, size_t len);
extern "C" SysRet<size_t> sys_vfs_size(CapIdx file_cap);
extern "C" SysRet<size_t> sys_vfs_getdents(CapIdx dir_cap, void *buf, size_t buflen,
                                           size_t offset);
extern "C" SysRet<void> sys_vfs_sync(CapIdx capidx);
extern "C" SysRet<void> sys_vfs_fchownat(CapIdx dirfd, uint32_t uid,
                                         uint32_t gid, uint32_t flags,
                                         const char *pathname);
extern "C" SysRet<void> sys_pipe_create(size_t nonblock,
                                        PipeCreateRet *out);
extern "C" SysRet<size_t> sys_pipe_read(CapIdx read_cap, void *buf,
                                        size_t len);
extern "C" SysRet<size_t> sys_pipe_write(CapIdx write_cap, const void *buf,
                                         size_t len);
