#include <sustcore/syscall.h>

    .text

    .macro do_syscall
        syscall 0
        ret
    .endm

    .global sys_write_serial
    .type sys_write_serial, @function
sys_write_serial:
    /* $a0 = __always_zero, $a1 = str, $a2 = len */
    li.d $a7, SYS_WRITE_SERIAL
    do_syscall

    .global sys_shutdown
    .type sys_shutdown, @function
sys_shutdown:
    move $a0, $zero
    li.d $a7, SYS_SHUTDOWN
    do_syscall

    .global sys_tcb_kill
    .type sys_tcb_kill, @function
sys_tcb_kill:
    li.d $a7, SYS_TCB_KILL
    do_syscall

    .global sys_pcb_kill
    .type sys_pcb_kill, @function
sys_pcb_kill:
    li.d $a7, SYS_PCB_KILL
    do_syscall

    .global sys_pcb_create_process
    .type sys_pcb_create_process, @function
sys_pcb_create_process:
    li.d $a7, SYS_CREATE_PROCESS
    do_syscall

    .global sys_pcb_create_linux_process
    .type sys_pcb_create_linux_process, @function
sys_pcb_create_linux_process:
    li.d $a7, SYS_CREATE_POSIX_PROCESS
    do_syscall

    .global sys_pcb_create_thread
    .type sys_pcb_create_thread, @function
sys_pcb_create_thread:
    li.d $a7, SYS_PCB_CREATE_THREAD
    do_syscall

    .global sys_tcb_wait
    .type sys_tcb_wait, @function
sys_tcb_wait:
    /* $a0 = tcb cap slot, $a1 = pcb cap array, $a2 = status out ptr, $a3 = options */
    li.d $a7, SYS_TCB_WAIT
    do_syscall

    .global sys_pcb_fork
    .type sys_pcb_fork, @function
sys_pcb_fork:
    move $t0, $a1
    li.d $a7, SYS_PCB_FORK
    syscall 0
    beqz $a1, 1f
    beqz $t0, 1f
    li.d $t1, -1
    st.d $t1, $t0, 0
1:
    ret

    .global sys_pcb_execve
    .type sys_pcb_execve, @function
sys_pcb_execve:
    li.d $a7, SYS_PCB_EXECVE
    do_syscall

    .global sys_pcb_execve_linux
    .type sys_pcb_execve_linux, @function
sys_pcb_execve_linux:
    li.d $a7, SYS_PCB_EXECVE_POSIX
    do_syscall

    .global sys_notif_wait
    .type sys_notif_wait, @function
sys_notif_wait:
    li.d $a7, SYS_NOTIF_WAIT
    do_syscall

    .global sys_notif_signal
    .type sys_notif_signal, @function
sys_notif_signal:
    li.d $a7, SYS_NOTIF_SIGNAL
    do_syscall

    .global sys_notif_unsignal
    .type sys_notif_unsignal, @function
sys_notif_unsignal:
    li.d $a7, SYS_NOTIF_UNSIGNAL
    do_syscall

    .global sys_notif_check
    .type sys_notif_check, @function
sys_notif_check:
    li.d $a7, SYS_NOTIF_CHECK
    do_syscall

    .global sys_cap_clone
    .type sys_cap_clone, @function
sys_cap_clone:
    li.d $a7, SYS_CAP_CLONE
    do_syscall

    .global sys_cap_downgrade
    .type sys_cap_downgrade, @function
sys_cap_downgrade:
    li.d $a7, SYS_CAP_DOWNGRADE
    do_syscall

    .global sys_cap_derive
    .type sys_cap_derive, @function
sys_cap_derive:
    li.d $a7, SYS_CAP_DERIVE
    do_syscall

    .global sys_cap_lookup
    .type sys_cap_lookup, @function
sys_cap_lookup:
    li.d $a7, SYS_CAP_LOOKUP
    do_syscall

    .global sys_cap_remove
    .type sys_cap_remove, @function
sys_cap_remove:
    li.d $a7, SYS_CAP_REMOVE
    do_syscall

    .global sys_getpid
    .type sys_getpid, @function
sys_getpid:
    li.d $a7, SYS_PCB_GETPID
    do_syscall

    .global sys_notif_create
    .type sys_notif_create, @function
sys_notif_create:
    move $a0, $zero
    li.d $a7, SYS_NOTIF_CREATE
    do_syscall

    .global sys_endpoint_create
    .type sys_endpoint_create, @function
sys_endpoint_create:
    move $a0, $zero
    li.d $a7, SYS_ENDPOINT_CREATE
    do_syscall

    .global sys_endpoint_send
    .type sys_endpoint_send, @function
sys_endpoint_send:
    li.d $a7, SYS_ENDPOINT_SEND
    do_syscall

    .global sys_endpoint_recv
    .type sys_endpoint_recv, @function
sys_endpoint_recv:
    li.d $a7, SYS_ENDPOINT_RECV
    do_syscall

    .global sys_endpoint_send_async
    .type sys_endpoint_send_async, @function
sys_endpoint_send_async:
    li.d $a7, SYS_ENDPOINT_SEND_ASYNC
    do_syscall

    .global sys_endpoint_recv_async
    .type sys_endpoint_recv_async, @function
sys_endpoint_recv_async:
    li.d $a7, SYS_ENDPOINT_RECV_ASYNC
    do_syscall

    .global endpoint_call
    .type endpoint_call, @function
endpoint_call:
    li.d $a7, SYS_ENDPOINT_CALL
    do_syscall

    .global endpoint_reply
    .type endpoint_reply, @function
endpoint_reply:
    li.d $a7, SYS_ENDPOINT_REPLY
    do_syscall

    .global sys_mem_create
    .type sys_mem_create, @function
sys_mem_create:
    /* $a0 = file cap, $a1 = memsz, $a2 = shared, $a3 = continuity, $a4 = growth, $a5 = file offset */
    li.d $a7, SYS_MEM_CREATE
    do_syscall

    .global sys_pcb_map
    .type sys_pcb_map, @function
sys_pcb_map:
    li.d $a7, SYS_PCB_MAP
    do_syscall

    .global sys_mem_unmap
    .type sys_mem_unmap, @function
sys_mem_unmap:
    li.d $a7, SYS_MEM_UNMAP
    do_syscall

    .global sys_mem_resize
    .type sys_mem_resize, @function
sys_mem_resize:
    li.d $a7, SYS_MEM_RESIZE
    do_syscall

    .global sys_mem_query
    .type sys_mem_query, @function
sys_mem_query:
    li.d $a7, SYS_MEM_QUERY
    do_syscall

    .global sys_vfs_opendir
    .type sys_vfs_opendir, @function
sys_vfs_opendir:
    li.d $a7, SYS_VFS_OPENDIR
    do_syscall

    .global sys_vfs_open
    .type sys_vfs_open, @function
sys_vfs_open:
    li.d $a7, SYS_VFS_OPEN
    do_syscall

    .global sys_vfs_mkfile
    .type sys_vfs_mkfile, @function
sys_vfs_mkfile:
    li.d $a7, SYS_VFS_MKFILE
    do_syscall

    .global sys_vfs_mkdir
    .type sys_vfs_mkdir, @function
sys_vfs_mkdir:
    li.d $a7, SYS_VFS_MKDIR
    do_syscall

    .global sys_vfs_read
    .type sys_vfs_read, @function
sys_vfs_read:
    li.d $a7, SYS_VFS_READ
    do_syscall

    .global sys_vfs_write
    .type sys_vfs_write, @function
sys_vfs_write:
    li.d $a7, SYS_VFS_WRITE
    do_syscall

    .global sys_vfs_size
    .type sys_vfs_size, @function
sys_vfs_size:
    li.d $a7, SYS_VFS_SIZE
    do_syscall

    .global sys_vfs_getdents
    .type sys_vfs_getdents, @function
sys_vfs_getdents:
    li.d $a7, SYS_VFS_GETDENTS
    do_syscall

    .global sys_vfs_sync
    .type sys_vfs_sync, @function
sys_vfs_sync:
    li.d $a7, SYS_VFS_SYNC
    do_syscall

    .global sys_vfs_unlink
    .type sys_vfs_unlink, @function
sys_vfs_unlink:
    li.d $a7, SYS_VFS_UNLINK
    do_syscall

    .global sys_vfs_rmdir
    .type sys_vfs_rmdir, @function
sys_vfs_rmdir:
    li.d $a7, SYS_VFS_RMDIR
    do_syscall

    .global sys_vfs_truncate
    .type sys_vfs_truncate, @function
sys_vfs_truncate:
    li.d $a7, SYS_VFS_TRUNCATE
    do_syscall

    .global sys_vfs_rename
    .type sys_vfs_rename, @function
sys_vfs_rename:
    li.d $a7, SYS_VFS_RENAME
    do_syscall

    .global sys_vfs_symlink
    .type sys_vfs_symlink, @function
sys_vfs_symlink:
    li.d $a7, SYS_VFS_SYMLINK
    do_syscall

    .global sys_vfs_link
    .type sys_vfs_link, @function
sys_vfs_link:
    li.d $a7, SYS_VFS_LINK
    do_syscall

    .global sys_vfs_stat
    .type sys_vfs_stat, @function
sys_vfs_stat:
    li.d $a7, SYS_VFS_STAT
    do_syscall

    .global sys_vfs_lstat
    .type sys_vfs_lstat, @function
sys_vfs_lstat:
    li.d $a7, SYS_VFS_LSTAT
    do_syscall

    .global sys_vfs_fstat
    .type sys_vfs_fstat, @function
sys_vfs_fstat:
    li.d $a7, SYS_VFS_FSTAT
    do_syscall

    .global sys_vfs_readlink
    .type sys_vfs_readlink, @function
sys_vfs_readlink:
    li.d $a7, SYS_VFS_READLINK
    do_syscall

    .global sys_mnt_create
    .type sys_mnt_create, @function
sys_mnt_create:
    li.d $a7, SYS_MNT_CREATE
    do_syscall

    .global sys_mnt_mount
    .type sys_mnt_mount, @function
sys_mnt_mount:
    li.d $a7, SYS_MNT_MOUNT
    do_syscall

    .global sys_mnt_umount
    .type sys_mnt_umount, @function
sys_mnt_umount:
    li.d $a7, SYS_MNT_UMOUNT
    do_syscall

    .global sys_mnt_root
    .type sys_mnt_root, @function
sys_mnt_root:
    li.d $a7, SYS_MNT_ROOT
    do_syscall

    .global sys_mnt_state
    .type sys_mnt_state, @function
sys_mnt_state:
    li.d $a7, SYS_MNT_STATE
    do_syscall

    .global sys_vfs_page_cache_stats
    .type sys_vfs_page_cache_stats, @function
sys_vfs_page_cache_stats:
    li.d $a7, SYS_VFS_PAGE_CACHE_STATS
    do_syscall

    .global sys_time_now_ns
    .type sys_time_now_ns, @function
sys_time_now_ns:
    li.d $a7, SYS_TIME_NOW_NS
    do_syscall

    .global sys_tcb_nanosleep
    .type sys_tcb_nanosleep, @function
sys_tcb_nanosleep:
    move $a1, $a0
    move $a0, $zero
    li.d $a7, SYS_TCB_NANOSLEEP
    do_syscall
