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
#include <sustcore/attr.h>
#include <sustcore/bootstrap.h>
#include <sustcore/capability.h>
#include <sustcore/execve.h>
#include <sustcore/files.h>
#include <sustcore/msg.h>
#include <sustcore/sysret.h>

extern CapIdx __pcb_cap;
extern CapIdx __main_tcb_cap;
extern CapIdx __heap_mem_cap;
extern CapIdx __stack_mem_cap;
extern size_t __argc;
extern const char **__argv;
extern const char **__envp;
extern size_t __bsargc;
extern const bsheader **__bsargv;

enum KmodSchedClass : size_t {
    SCHED_CLASS_IDLE = 1,
    SCHED_CLASS_FCFS = 2,
    SCHED_CLASS_RR   = 3,
    SCHED_CLASS_RT   = 5,
};

constexpr size_t WNOHANG = 1;

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

struct KmodSigAction {
    size_t handler;
    uint64_t mask;
    uint64_t flags;
    size_t restorer;
};

extern "C" {
SysRet<void> sys_write_serial(size_t __always_zero, const char *str, size_t len);
SysRet<void> sys_shutdown();
SysRet<size_t> sys_time_now_ns();
SysRet<size_t> sys_getrtctime();
SysRet<void> sys_tcb_nanosleep(size_t ns);
SysRet<size_t> sys_tcb_get_tid(CapIdx tcb_cap);
SysRet<void> sys_tcb_kill(CapIdx tcb_cap, int exit_code);
SysRet<void> sys_pcb_kill(CapIdx pcb_cap, int exit_code);
SysRet<void> sys_pcb_map(CapIdx pcb_cap, CapIdx mem_cap, size_t offset, void *vaddr,
                 size_t sz, uint64_t protflg);
SysRet<void> sys_pcb_unmap(CapIdx pcb_cap, void *vaddr, size_t sz);
SysRet<size_t> sys_pcb_query_vaddr(CapIdx pcb_cap, void *vaddr, VMAInfo *info);
SysRet<size_t> sys_pcb_query_vspace(CapIdx pcb_cap, size_t offset, VMAInfo *info_array,
                            size_t max_entries);
SysRet<CapIdx> sys_pcb_create_process(CapIdx pcb_cap, size_t sched_class,
                                      const ExecveRequest *request);
SysRet<CapIdx> sys_pcb_create_linux_process(CapIdx pcb_cap, size_t sched_class,
                                            const ExecveRequest *request);
SysRet<CapIdx> sys_create_process(size_t sched_class,
                                  const ExecveRequest *request);
SysRet<CapIdx> sys_create_linux_process(size_t sched_class,
                                        const ExecveRequest *request);
SysRet<CapIdx> sys_pcb_create_thread(CapIdx pcb_cap, void (*entry)(),
                                     void *stack_addr, size_t stack_size);
SysRet<CapIdx> sys_create_thread(void (*entry)(), void *stack_addr, size_t stack_size);
SysRet<CapIdx> sys_tcb_wait(CapIdx tcb_cap, CapIdx pcbs_idx[], int *status,
                            size_t options);
SysRet<CapIdx> sys_tcb_timeout_wait(CapIdx tcb_cap, CapIdx pcbs_idx[],
                                    int *status, size_t timeout_ns,
                                    size_t options);
SysRet<size_t> sys_pcb_fork(CapIdx pcb_cap, CapIdx *child_pcb_cap);
SysRet<size_t> fork(CapIdx *child_pcb_cap);
SysRet<void> sys_pcb_execve(CapIdx pcb_cap, const ExecveRequest *request);
SysRet<void> sys_pcb_execve_linux(CapIdx pcb_cap,
                                  const ExecveRequest *request);
SysRet<CapIdx> sys_pcb_procfs_get(CapIdx pcb_cap, const char *name);
SysRet<void> sys_pcb_procfs_redirect(CapIdx pcb_cap, const char *name,
                                     const char *target);
SysRet<void> sys_pcb_sigaction(CapIdx pcb_cap, size_t signo,
                               const KmodSigAction *action,
                               KmodSigAction *old_action);
SysRet<void> sys_pcb_signal(CapIdx pcb_cap, size_t signo);
SysRet<size_t> sys_pcb_waitsig(CapIdx pcb_cap, uint64_t mask,
                               size_t timeout_ns, size_t options);
SysRet<void> sys_execve(const ExecveRequest *request);
SysRet<void> execve(const ExecveRequest *request);

SysRet<CapIdx> sys_vfs_opendir(CapIdx parent_dir_cap, const char *path,
                               flags::oflg_t oflags);
SysRet<CapIdx> sys_vfs_open(CapIdx parent_dir_cap, const char *path,
                            flags::oflg_t oflags);
SysRet<CapIdx> sys_vfs_mkfile(CapIdx parent_dir_cap, const char *path,
                              flags::oflg_t oflags);
SysRet<CapIdx> sys_vfs_mkdir(CapIdx parent_dir_cap, const char *path,
                             flags::oflg_t oflags);
SysRet<void> sys_vfs_unlink(CapIdx parent_dir_cap, const char *name);
SysRet<void> sys_vfs_rmdir(CapIdx parent_dir_cap, const char *name);
SysRet<void> sys_vfs_truncate(CapIdx file_cap, size_t new_size);
SysRet<void> sys_vfs_rename(CapIdx old_parent_cap, const char *old_name,
                            CapIdx new_parent_cap, const char *new_name);
SysRet<void> sys_vfs_symlink(CapIdx parent_dir_cap, const char *name,
                             const char *target);
SysRet<void> sys_vfs_link(CapIdx parent_dir_cap, const char *name, CapIdx target);
SysRet<void> sys_vfs_stat(CapIdx parent_dir_cap, const char *name, NodeMeta *out);
SysRet<void> sys_vfs_lstat(CapIdx parent_dir_cap, const char *name, NodeMeta *out);
SysRet<void> sys_vfs_fstat(CapIdx file_cap, NodeMeta *out);
SysRet<void> sys_vfs_statfs(CapIdx capidx, VFSStatFS *out);
SysRet<void> sys_vfs_ioctl(CapIdx file_cap, size_t cmd, void *arg, size_t arg_len);
SysRet<void> sys_vfs_getattr(CapIdx capidx, AttrSet *out);
SysRet<void> sys_vfs_getattr_at(CapIdx parent_dir_cap, const char *name,
                                AttrSet *out, uint32_t flags);
SysRet<void> sys_vfs_setattr(CapIdx capidx, const AttrSet *attrs,
                             uint32_t mask, uint32_t flags);
SysRet<void> sys_vfs_setattr_at(CapIdx parent_dir_cap, const char *name,
                                const AttrSet *attrs, uint32_t mask,
                                uint32_t flags);
SysRet<void> sys_vfs_chown(CapIdx fd, uint32_t uid, uint32_t gid,
                           uint32_t flags);
SysRet<void> sys_vfs_chown_at(CapIdx dirfd, uint32_t uid, uint32_t gid,
                              uint32_t flags, const char *pathname);
SysRet<size_t> sys_vfs_readlink(CapIdx parent_dir_cap, const char *name, char *buf,
                                size_t bufsiz);
SysRet<void> sys_vfs_fchownat(CapIdx dirfd, uint32_t uid, uint32_t gid,
                              uint32_t flags, const char *pathname);
SysRet<CapIdx> sys_mnt_create(CapIdx devfile_cap, const char *fs_name,
                              uint64_t superflags, const char *options);
SysRet<void> sys_mnt_mount(CapIdx mntcap, CapIdx parent_dir_cap, const char *mountpoint,
                           uint64_t attachflags);
SysRet<void> sys_mnt_umount(CapIdx mntcap, uint64_t flags);
SysRet<CapIdx> sys_mnt_root(CapIdx mntcap);
SysRet<MountStatus> sys_mnt_state(CapIdx mntcap);
SysRet<size_t> sys_vfs_read(CapIdx file_cap, size_t offset, void *buf, size_t len);
SysRet<size_t> sys_vfs_write(CapIdx file_cap, size_t offset, const void *buf,
                             size_t len);
SysRet<size_t> sys_vfs_size(CapIdx file_cap);
/**
 * @brief 读取目录项记录到缓冲区, 返回本次实际写入的字节数.
 *
 * @param offset 目录项起始索引, 不是字节偏移.
 */
SysRet<size_t> sys_vfs_getdents(CapIdx dir_cap, void *buf, size_t buflen,
                                size_t offset);
SysRet<void> sys_vfs_sync(CapIdx capidx);
SysRet<void> sys_vfs_page_cache_stats(size_t __always_zero, VFSPageCacheStats *out,
                              bool reset);

SysRet<CapIdx> sys_cap_clone(CapIdx src);
SysRet<void> sys_cap_downgrade(CapIdx idx, uint64_t new_perm);
SysRet<CapIdx> sys_cap_derive(CapIdx src, uint64_t new_perm);
SysRet<void> sys_cap_remove(CapIdx idx);
SysRet<void> sys_cap_lookup(CapIdx idx, CapInfo *info);
SysRet<size_t> sys_getpid(CapIdx pcb_cap);

SysRet<CapIdx> sys_notif_create();
SysRet<void> sys_notif_signal(CapIdx capidx, size_t idx);
SysRet<void> sys_notif_unsignal(CapIdx capidx, size_t idx);
SysRet<bool> sys_notif_check(CapIdx capidx, size_t idx);
SysRet<void> sys_notif_wait(CapIdx capidx, size_t idx);

SysRet<CapIdx> sys_endpoint_create();
/**
 * @brief 阻塞地向endpoint发送一条MsgPacket描述的消息.
 */
SysRet<void> sys_endpoint_send(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 阻塞地从endpoint接收一条消息并写回MsgPacket描述的缓冲区.
 */
SysRet<void> sys_endpoint_recv(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 非阻塞地向endpoint发送一条消息.
 */
SysRet<bool> sys_endpoint_send_async(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 非阻塞地从endpoint接收一条消息.
 */
SysRet<bool> sys_endpoint_recv_async(CapIdx endpoint, MsgPacket *packet);
/**
 * @brief 发起一次同步endpoint调用, 自动携带一次性Reply Capability.
 *
 * @param endpoint 目标endpoint capability.
 * @param sendmsg 请求消息.
 * @param replymsg 用于接收回复消息的缓冲区描述符.
 */
SysRet<void> endpoint_call(CapIdx endpoint, MsgPacket *sendmsg, MsgPacket *replymsg);
/**
 * @brief 使用Reply Capability回复一次endpoint_call.
 *
 * 成功回复后, reply_cap 会从当前CSpace中移除.
 */
SysRet<void> endpoint_reply(CapIdx reply_cap, MsgPacket *replymsg);

SysRet<CapIdx> sys_mem_create(CapIdx file_cap, size_t memsz, bool shared,
                              bool continuity, uint64_t growth, size_t file_offset);
SysRet<void> sys_mem_map(CapIdx idx, void *vaddr, uint64_t rwx, uint64_t growth);
SysRet<void> sys_mem_unmap(CapIdx idx, void *vaddr);
SysRet<void> sys_mem_resize(CapIdx idx, size_t newsz);
SysRet<void> sys_mem_sync(CapIdx idx, size_t offset, size_t len);
SysRet<void> sys_mem_query(CapIdx idx, MemQueryRet *out);
}

extern "C" {
int kputs(const char *str);
size_t brk(size_t newbrk);
void *sbrk(ptrdiff_t increment);
void exit(int exit_code);

int kmod_fopen(const char *path, const char *options);
int kmod_opendir(const char *path);
size_t kmod_fread(int fd, void *buf, size_t len);
size_t kmod_fwrite(int fd, const void *buf, size_t len);
int kmod_mkdir(const char *path);
int kmod_mkfile(const char *path, const char *options);
int kmod_unlink(const char *path);
int kmod_rmdir(const char *path);
int kmod_truncate(const char *path, size_t new_size);
int kmod_rename(const char *old_path, const char *new_path);
int kmod_symlink(const char *path, const char *target);
int kmod_link(const char *path, const char *target_path);
CapIdx kmod_getcap(int fd);
void kmod_fclose(int fd);
}
