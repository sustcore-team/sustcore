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
#include <sustcore/files.h>
#include <sustcore/msg.h>

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

extern "C" void sys_write_serial(size_t __always_zero, const char *str,
                                 size_t len);
extern "C" bool sys_pcb_kill(CapIdx pcb_cap, int exit_code);
extern "C" size_t sys_pcb_query_vaddr(CapIdx pcb_cap, void *vaddr,
                                      VMAInfo *info);
extern "C" CapIdx sys_pcb_create_process(CapIdx pcb_cap, CapIdx image_cap,
                                         size_t sched_class, CapIdx caps[],
                                         const char *argv[],
                                         const char *envp[],
                                         const char *bsargv[]);
extern "C" CapIdx sys_pcb_create_linux_process(CapIdx pcb_cap,
                                               CapIdx image_cap,
                                               size_t sched_class,
                                               CapIdx caps[],
                                               const char *argv[],
                                               const char *envp[],
                                               const char *bsargv[]);
extern "C" CapIdx sys_pcb_create_thread(CapIdx pcb_cap, void (*entry)(),
                                        void *stack_addr,
                                        size_t stack_size);
extern "C" CapIdx sys_tcb_wait(CapIdx tcb_cap, CapIdx pcbs_idx[],
                               int *status, size_t options);
extern "C" size_t sys_pcb_fork(CapIdx pcb_cap, CapIdx *child_pcb_cap);
extern "C" bool sys_pcb_execve(CapIdx pcb_cap, CapIdx image_cap,
                               CapIdx rsvdlst[], const char *argv[],
                               const char *envp[], const char *bsargv[]);
extern "C" size_t sys_yield();
extern "C" size_t sys_getpid(CapIdx pcb_cap);
extern "C" CapIdx sys_notif_create();
extern "C" bool sys_notif_signal(CapIdx capidx, size_t idx);
extern "C" bool sys_notif_unsignal(CapIdx capidx, size_t idx);
extern "C" bool sys_notif_check(CapIdx capidx, size_t idx);
extern "C" bool sys_notif_wait(CapIdx capidx, size_t idx);
extern "C" CapIdx sys_cap_clone(CapIdx src);
extern "C" bool sys_cap_downgrade(CapIdx idx, uint64_t new_perm);
extern "C" CapIdx sys_cap_derive(CapIdx src, uint64_t new_perm);
extern "C" bool sys_cap_remove(CapIdx idx);
extern "C" bool sys_cap_lookup(CapIdx idx, CapInfo *info);
extern "C" CapIdx sys_endpoint_create();
extern "C" void sys_endpoint_send(CapIdx endpoint, MsgPacket *packet);
extern "C" void sys_endpoint_recv(CapIdx endpoint, MsgPacket *packet);
extern "C" bool sys_endpoint_send_async(CapIdx endpoint, MsgPacket *packet);
extern "C" bool sys_endpoint_recv_async(CapIdx endpoint, MsgPacket *packet);
extern "C" void endpoint_call(CapIdx endpoint, MsgPacket *sendmsg,
                              MsgPacket *replymsg);
extern "C" void endpoint_reply(CapIdx reply_cap, MsgPacket *replymsg);
extern "C" CapIdx sys_mem_create(size_t __always_zero, size_t memsz,
                                 bool shared, bool continuity,
                                 uint64_t growth);
extern "C" bool sys_mem_unmap(CapIdx idx, void *vaddr);
extern "C" bool sys_mem_resize(CapIdx idx, size_t newsz);
extern "C" bool sys_mem_query(CapIdx idx, MemQueryRet *out);
extern "C" bool sys_pcb_map(CapIdx pcb_cap, CapIdx mem_cap, size_t offset,
                            void *vaddr, size_t sz, uint64_t protflg);
extern "C" bool sys_pcb_unmap(CapIdx pcb_cap, void *vaddr, size_t sz);
extern "C" size_t sys_pcb_query_vspace(CapIdx pcb_cap, size_t offset,
                                       VMAInfo *info_array,
                                       size_t max_entries);
extern "C" CapIdx sys_vfs_opendir(CapIdx parent_dir_cap, const char *path,
                                  flags::oflg_t oflags);
extern "C" CapIdx sys_vfs_open(CapIdx parent_dir_cap, const char *path,
                               flags::oflg_t oflags);
extern "C" CapIdx sys_vfs_mkfile(CapIdx parent_dir_cap, const char *path,
                                 flags::oflg_t oflags);
extern "C" CapIdx sys_vfs_mkdir(CapIdx parent_dir_cap, const char *path,
                                flags::oflg_t oflags);
extern "C" bool sys_vfs_unlink(CapIdx parent_dir_cap, const char *name);
extern "C" bool sys_vfs_rmdir(CapIdx parent_dir_cap, const char *name);
extern "C" bool sys_vfs_truncate(CapIdx file_cap, size_t new_size);
extern "C" bool sys_vfs_rename(CapIdx old_parent_cap, const char *old_name,
                               CapIdx new_parent_cap, const char *new_name);
extern "C" bool sys_vfs_symlink(CapIdx parent_dir_cap, const char *name,
                                const char *target);
extern "C" bool sys_vfs_link(CapIdx parent_dir_cap, const char *name,
                             CapIdx target);
extern "C" bool sys_vfs_stat(CapIdx parent_dir_cap, const char *name,
                             NodeMeta *out);
extern "C" bool sys_vfs_lstat(CapIdx parent_dir_cap, const char *name,
                              NodeMeta *out);
extern "C" size_t sys_vfs_readlink(CapIdx parent_dir_cap, const char *name,
                                   char *buf, size_t bufsiz);
extern "C" size_t sys_vfs_read(CapIdx file_cap, size_t offset, void *buf,
                               size_t len);
extern "C" size_t sys_vfs_write(CapIdx file_cap, size_t offset,
                                const void *buf, size_t len);
extern "C" size_t sys_vfs_size(CapIdx file_cap);
extern "C" size_t sys_vfs_getdents(CapIdx dir_cap, void *buf, size_t buflen,
                                   size_t offset);
extern "C" bool sys_vfs_sync(CapIdx capidx);
