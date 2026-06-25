#include <sustcore/syscall.h>

    .text
    .global sys_write_serial
    .type   sys_write_serial, @function
sys_write_serial:
    /* a0 = __always_zero, a1 = str, a2 = len */
    li a7, SYS_WRITE_SERIAL
    ecall
    ret

    .global sys_shutdown
    .type   sys_shutdown, @function
sys_shutdown:
    li a0, 0
    li a7, SYS_SHUTDOWN
    ecall
    ret

    .global sys_pcb_kill
    .type sys_pcb_kill, @function
sys_pcb_kill:
    /* a0 = pcb cap slot, a1 = exit code */
    li a7, SYS_PCB_KILL
    ecall
    ret

    .global sys_pcb_create_process
    .type   sys_pcb_create_process, @function
sys_pcb_create_process:
    /* a0 = pcb cap slot, a1 = image cap, a2 = initial caps, a3 = size, a4 = sched,
       a5 = startup blob ptr, a6 = startup blob size */
    li a7, SYS_CREATE_PROCESS
    ecall
    ret

    .global sys_pcb_create_linux_process
    .type   sys_pcb_create_linux_process, @function
sys_pcb_create_linux_process:
    /* a0 = pcb cap slot, a1 = image cap, a2 = initial caps, a3 = size, a4 = sched,
       a5 = startup blob ptr, a6 = startup blob size */
    li a7, SYS_CREATE_POSIX_PROCESS
    ecall
    ret

    .global sys_pcb_create_thread
    .type   sys_pcb_create_thread, @function
sys_pcb_create_thread:
    /* a0 = pcb cap slot, a1 = entry, a2 = stack_addr, a3 = stack_size */
    li a7, SYS_CREATE_THREAD
    ecall
    ret

    .global sys_tcb_wait
    .type   sys_tcb_wait, @function
sys_tcb_wait:
    /* a0 = tcb cap slot, a1 = pcb cap array, a2 = status out ptr, a3 = options */
    li a7, SYS_TCB_WAIT
    ecall
    ret

    .global sys_pcb_fork
    .type   sys_pcb_fork, @function
sys_pcb_fork:
    /* a0 = pcb cap slot, a1 = child PCB cap out ptr */
    mv t0, a1
    li a7, SYS_FORK
    ecall
    beqz a1, 1f
    beqz t0, 1f
    li t1, -1
    sd t1, 0(t0)
1:
    ret

    .global sys_pcb_execve
    .type   sys_pcb_execve, @function
sys_pcb_execve:
    /* a0 = pcb cap slot, a1 = image cap, a2 = reserved caps, a3 = reserved caps size,
       a4 = startup blob ptr, a5 = startup blob size */
    li a7, SYS_EXECVE
    ecall
    ret

    .global sys_notif_wait
    .type sys_notif_wait, @function
sys_notif_wait:
    /* a0 = cap slot, a1 = signal idx */
    li a7, SYS_NOTIF_WAIT
    ecall
    ret

    .global sys_notif_signal
    .type sys_notif_signal, @function
sys_notif_signal:
    /* a0 = cap slot, a1 = signal idx */
    li a7, SYS_NOTIF_SIGNAL
    ecall
    ret

    .global sys_notif_unsignal
    .type sys_notif_unsignal, @function
sys_notif_unsignal:
    /* a0 = cap slot, a1 = signal idx */
    li a7, SYS_NOTIF_UNSIGNAL
    ecall
    ret

    .global sys_notif_check
    .type sys_notif_check, @function
sys_notif_check:
    /* a0 = cap slot, a1 = signal idx */
    li a7, SYS_NOTIF_CHECK
    ecall
    ret

    .global sys_cap_clone
    .type sys_cap_clone, @function
sys_cap_clone:
    /* a0 = src cap slot */
    li a7, SYS_CAP_CLONE
    ecall
    ret

    .global sys_cap_downgrade
    .type sys_cap_downgrade, @function
sys_cap_downgrade:
    /* a0 = cap slot, a1 = new permission bits */
    li a7, SYS_CAP_DOWNGRADE
    ecall
    ret

    .global sys_cap_derive
    .type sys_cap_derive, @function
sys_cap_derive:
    /* a0 = src cap slot, a1 = new permission bits */
    li a7, SYS_CAP_DERIVE
    ecall
    ret

    .global sys_cap_lookup
    .type sys_cap_lookup, @function
sys_cap_lookup:
    /* a0 = cap slot, a1 = CapInfo out ptr */
    li a7, SYS_CAP_LOOKUP
    ecall
    ret

    .global sys_cap_remove
    .type sys_cap_remove, @function
sys_cap_remove:
    /* a0 = cap slot */
    li a7, SYS_CAP_REMOVE
    ecall
    ret

    .global sys_getpid
    .type sys_getpid, @function
sys_getpid:
    /* a0 = pcb cap slot */
    li a7, SYS_GETPID
    ecall
    ret

    .global sys_notif_create
    .type sys_notif_create, @function
sys_notif_create:
    /* no args */
    li a0, 0
    li a7, SYS_NOTIF_CREATE
    ecall
    ret

    .global sys_endpoint_create
    .type sys_endpoint_create, @function
sys_endpoint_create:
    /* no args */
    li a0, 0
    li a7, SYS_ENDPOINT_CREATE
    ecall
    ret

    .global sys_endpoint_send
    .type sys_endpoint_send, @function
sys_endpoint_send:
    /* a0 = endpoint, a1 = MsgPacket* */
    li a7, SYS_ENDPOINT_SEND
    ecall
    ret

    .global sys_endpoint_recv
    .type sys_endpoint_recv, @function
sys_endpoint_recv:
    /* a0 = endpoint, a1 = MsgPacket* */
    li a7, SYS_ENDPOINT_RECV
    ecall
    ret

    .global sys_endpoint_send_async
    .type sys_endpoint_send_async, @function
sys_endpoint_send_async:
    /* a0 = endpoint, a1 = MsgPacket* */
    li a7, SYS_ENDPOINT_SEND_ASYNC
    ecall
    ret

    .global sys_endpoint_recv_async
    .type sys_endpoint_recv_async, @function
sys_endpoint_recv_async:
    /* a0 = endpoint, a1 = MsgPacket* */
    li a7, SYS_ENDPOINT_RECV_ASYNC
    ecall
    ret

    .global endpoint_call
    .type endpoint_call, @function
endpoint_call:
    /* a0 = endpoint, a1 = send MsgPacket*, a2 = reply MsgPacket* */
    li a7, SYS_ENDPOINT_CALL
    ecall
    ret

    .global endpoint_reply
    .type endpoint_reply, @function
endpoint_reply:
    /* a0 = reply cap, a1 = reply MsgPacket* */
    li a7, SYS_ENDPOINT_REPLY
    ecall
    ret

    .global sys_mem_create
    .type sys_mem_create, @function
sys_mem_create:
    /* a0 = file cap, a1 = memsz, a2 = shared, a3 = continuity, a4 = growth, a5 = file offset */
    li a7, SYS_MEM_CREATE
    ecall
    ret

    .global sys_pcb_map
    .type sys_pcb_map, @function
sys_pcb_map:
    /* a0 = pcb cap slot, a1 = mem cap slot, a2 = vaddr, a3 = rwx, a4 = growth */
    li a7, SYS_PCB_MAP
    ecall
    ret

    .global sys_mem_unmap
    .type sys_mem_unmap, @function
sys_mem_unmap:
    /* a0 = cap slot, a1 = vaddr */
    li a7, SYS_MEM_UNMAP
    ecall
    ret

    .global sys_mem_resize
    .type sys_mem_resize, @function
sys_mem_resize:
    /* a0 = cap slot, a1 = newsz */
    li a7, SYS_MEM_RESIZE
    ecall
    ret

    .global sys_mem_query
    .type sys_mem_query, @function
sys_mem_query:
    /* a0 = cap slot, a1 = MemQueryRet out ptr */
    li a7, SYS_MEM_QUERY
    ecall
    ret

    .global sys_vfs_opendir
    .type sys_vfs_opendir, @function
sys_vfs_opendir:
    /* a0 = parent dir cap, a1 = path, a2 = oflags */
    li a7, SYS_VFS_OPENDIR
    ecall
    ret

    .global sys_vfs_open
    .type sys_vfs_open, @function
sys_vfs_open:
    /* a0 = parent dir cap, a1 = path, a2 = oflags */
    li a7, SYS_VFS_OPEN
    ecall
    ret

    .global sys_vfs_mkfile
    .type sys_vfs_mkfile, @function
sys_vfs_mkfile:
    /* a0 = parent dir cap, a1 = path, a2 = oflags */
    li a7, SYS_VFS_MKFILE
    ecall
    ret

    .global sys_vfs_mkdir
    .type sys_vfs_mkdir, @function
sys_vfs_mkdir:
    /* a0 = parent dir cap, a1 = path, a2 = oflags */
    li a7, SYS_VFS_MKDIR
    ecall
    ret

    .global sys_vfs_unlink
    .type sys_vfs_unlink, @function
sys_vfs_unlink:
    /* a0 = parent dir cap, a1 = name */
    li a7, SYS_VFS_UNLINK
    ecall
    ret

    .global sys_vfs_rmdir
    .type sys_vfs_rmdir, @function
sys_vfs_rmdir:
    /* a0 = parent dir cap, a1 = name */
    li a7, SYS_VFS_RMDIR
    ecall
    ret

    .global sys_vfs_truncate
    .type sys_vfs_truncate, @function
sys_vfs_truncate:
    /* a0 = file cap, a1 = new_size */
    li a7, SYS_VFS_TRUNCATE
    ecall
    ret

    .global sys_vfs_rename
    .type sys_vfs_rename, @function
sys_vfs_rename:
    /* a0 = old_parent_cap, a1 = old_name, a2 = new_parent_cap, a3 = new_name */
    li a7, SYS_VFS_RENAME
    ecall
    ret

    .global sys_vfs_symlink
    .type sys_vfs_symlink, @function
sys_vfs_symlink:
    /* a0 = parent_dir_cap, a1 = name, a2 = target */
    li a7, SYS_VFS_SYMLINK
    ecall
    ret

    .global sys_vfs_link
    .type sys_vfs_link, @function
sys_vfs_link:
    /* a0 = parent_dir_cap, a1 = name, a2 = target_inode */
    li a7, SYS_VFS_LINK
    ecall
    ret

    .global sys_vfs_stat
    .type sys_vfs_stat, @function
sys_vfs_stat:
    /* a0 = parent_dir_cap, a1 = name, a2 = stat* */
    li a7, SYS_VFS_STAT
    ecall
    ret

    .global sys_vfs_lstat
    .type sys_vfs_lstat, @function
sys_vfs_lstat:
    /* a0 = parent_dir_cap, a1 = name, a2 = stat* */
    li a7, SYS_VFS_LSTAT
    ecall
    ret

    .global sys_vfs_readlink
    .type sys_vfs_readlink, @function
sys_vfs_readlink:
    /* a0 = parent_dir_cap, a1 = name, a2 = buf, a3 = bufsiz */
    li a7, SYS_VFS_READLINK
    ecall
    ret

    .global sys_vfs_mount
    .type sys_vfs_mount, @function
sys_vfs_mount:
    /* a0 = parent_dir_cap, a1 = fs_name, a2 = devfile_cap, a3 = mountpoint, a4 = flags, a5 = options */
    li a7, SYS_VFS_MOUNT
    ecall
    ret

    .global sys_vfs_read
    .type sys_vfs_read, @function
sys_vfs_read:
    /* a0 = file cap, a1 = offset, a2 = buf, a3 = len */
    li a7, SYS_VFS_READ
    ecall
    ret

    .global sys_vfs_write
    .type sys_vfs_write, @function
sys_vfs_write:
    /* a0 = file cap, a1 = offset, a2 = buf, a3 = len */
    li a7, SYS_VFS_WRITE
    ecall
    ret

    .global sys_vfs_size
    .type sys_vfs_size, @function
sys_vfs_size:
    /* a0 = file cap */
    li a7, SYS_VFS_SIZE
    ecall
    ret

    .global sys_vfs_getdents
    .type sys_vfs_getdents, @function
sys_vfs_getdents:
    /* a0 = dir cap, a1 = buf, a2 = buflen, a3 = offset */
    li a7, SYS_VFS_GETDENTS
    ecall
    ret

    .global sys_vfs_sync
    .type sys_vfs_sync, @function
sys_vfs_sync:
    /* a0 = cap */
    li a7, SYS_VFS_SYNC
    ecall
    ret

    .global sys_vfs_page_cache_stats
    .type sys_vfs_page_cache_stats, @function
sys_vfs_page_cache_stats:
    /* a0 = always zero, a1 = VFSPageCacheStats*, a2 = reset */
    li a7, SYS_VFS_PAGE_CACHE_STATS
    ecall
    ret

    .global sys_time_now_ns
    .type sys_time_now_ns, @function
sys_time_now_ns:
    li a7, SYS_TIME_NOW_NS
    ecall
    ret
