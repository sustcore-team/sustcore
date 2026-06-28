#include <sustcore/syscall.h>

    .text
    .global sys_write_serial
    .type   sys_write_serial, @function
sys_write_serial:
    /* a0 = __always_zero, a1 = str, a2 = len */
    li a7, SYS_WRITE_SERIAL
    ecall
    ret

    .global sys_time_now_ns
    .type   sys_time_now_ns, @function
sys_time_now_ns:
    li a7, SYS_TIME_NOW_NS
    ecall
    ret

    .global sys_tcb_nanosleep
    .type   sys_tcb_nanosleep, @function
sys_tcb_nanosleep:
    mv a1, a0
    li a0, 0
    li a7, SYS_TCB_NANOSLEEP
    ecall
    ret

    .global sys_tcb_kill
    .type   sys_tcb_kill, @function
sys_tcb_kill:
    li a7, SYS_TCB_KILL
    ecall
    ret

    .global sys_pcb_kill
    .type   sys_pcb_kill, @function
sys_pcb_kill:
    li a7, SYS_PCB_KILL
    ecall
    ret

    .global sys_pcb_query_vaddr
    .type   sys_pcb_query_vaddr, @function
sys_pcb_query_vaddr:
    li a7, SYS_PCB_QUERY_VADDR
    ecall
    ret

    .global sys_pcb_create_process
    .type   sys_pcb_create_process, @function
sys_pcb_create_process:
    li a7, SYS_CREATE_PROCESS
    ecall
    ret

    .global sys_pcb_create_linux_process
    .type   sys_pcb_create_linux_process, @function
sys_pcb_create_linux_process:
    li a7, SYS_CREATE_POSIX_PROCESS
    ecall
    ret

    .global sys_pcb_create_thread
    .type   sys_pcb_create_thread, @function
sys_pcb_create_thread:
    li a7, SYS_PCB_CREATE_THREAD
    ecall
    ret

    .global sys_tcb_wait
    .type   sys_tcb_wait, @function
sys_tcb_wait:
    li a7, SYS_TCB_WAIT
    ecall
    ret

    .global sys_tcb_get_tid
    .type   sys_tcb_get_tid, @function
sys_tcb_get_tid:
    li a7, SYS_TCB_GET_TID
    ecall
    ret

    .global sys_pcb_fork
    .type   sys_pcb_fork, @function
sys_pcb_fork:
    mv t0, a1
    li a7, SYS_PCB_FORK
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
    li a7, SYS_PCB_EXECVE
    ecall
    ret

    .global sys_pcb_execve_linux
    .type   sys_pcb_execve_linux, @function
sys_pcb_execve_linux:
    li a7, SYS_PCB_EXECVE_POSIX
    ecall
    ret

    .global sys_pcb_procfs_get
    .type   sys_pcb_procfs_get, @function
sys_pcb_procfs_get:
    li a7, SYS_PCB_PROCFS_GET
    ecall
    ret

    .global sys_pcb_procfs_redirect
    .type   sys_pcb_procfs_redirect, @function
sys_pcb_procfs_redirect:
    li a7, SYS_PCB_PROCFS_REDIRECT
    ecall
    ret

    .global sys_yield
    .type   sys_yield, @function
sys_yield:
    li a7, SYS_TCB_YIELD
    ecall
    ret

    .global sys_getpid
    .type   sys_getpid, @function
sys_getpid:
    li a7, SYS_PCB_GETPID
    ecall
    ret

    .global sys_notif_create
    .type   sys_notif_create, @function
sys_notif_create:
    li a0, 0
    li a7, SYS_NOTIF_CREATE
    ecall
    ret

    .global sys_notif_signal
    .type   sys_notif_signal, @function
sys_notif_signal:
    li a7, SYS_NOTIF_SIGNAL
    ecall
    ret

    .global sys_notif_unsignal
    .type   sys_notif_unsignal, @function
sys_notif_unsignal:
    li a7, SYS_NOTIF_UNSIGNAL
    ecall
    ret

    .global sys_notif_check
    .type   sys_notif_check, @function
sys_notif_check:
    li a7, SYS_NOTIF_CHECK
    ecall
    ret

    .global sys_notif_wait
    .type   sys_notif_wait, @function
sys_notif_wait:
    li a7, SYS_NOTIF_WAIT
    ecall
    ret

    .global sys_cap_clone
    .type   sys_cap_clone, @function
sys_cap_clone:
    li a7, SYS_CAP_CLONE
    ecall
    ret

    .global sys_cap_downgrade
    .type   sys_cap_downgrade, @function
sys_cap_downgrade:
    li a7, SYS_CAP_DOWNGRADE
    ecall
    ret

    .global sys_cap_derive
    .type   sys_cap_derive, @function
sys_cap_derive:
    li a7, SYS_CAP_DERIVE
    ecall
    ret

    .global sys_cap_lookup
    .type   sys_cap_lookup, @function
sys_cap_lookup:
    li a7, SYS_CAP_LOOKUP
    ecall
    ret

    .global sys_cap_remove
    .type   sys_cap_remove, @function
sys_cap_remove:
    li a7, SYS_CAP_REMOVE
    ecall
    ret

    .global sys_endpoint_create
    .type   sys_endpoint_create, @function
sys_endpoint_create:
    li a0, 0
    li a7, SYS_ENDPOINT_CREATE
    ecall
    ret

    .global sys_endpoint_send
    .type   sys_endpoint_send, @function
sys_endpoint_send:
    li a7, SYS_ENDPOINT_SEND
    ecall
    ret

    .global sys_endpoint_recv
    .type   sys_endpoint_recv, @function
sys_endpoint_recv:
    li a7, SYS_ENDPOINT_RECV
    ecall
    ret

    .global sys_endpoint_send_async
    .type   sys_endpoint_send_async, @function
sys_endpoint_send_async:
    li a7, SYS_ENDPOINT_SEND_ASYNC
    ecall
    ret

    .global sys_endpoint_recv_async
    .type   sys_endpoint_recv_async, @function
sys_endpoint_recv_async:
    li a7, SYS_ENDPOINT_RECV_ASYNC
    ecall
    ret

    .global endpoint_call
    .type   endpoint_call, @function
endpoint_call:
    li a7, SYS_ENDPOINT_CALL
    ecall
    ret

    .global endpoint_reply
    .type   endpoint_reply, @function
endpoint_reply:
    li a7, SYS_ENDPOINT_REPLY
    ecall
    ret

    .global sys_mem_resize
    .type   sys_mem_resize, @function
sys_mem_resize:
    li a7, SYS_MEM_RESIZE
    ecall
    ret

    .global sys_mem_create
    .type   sys_mem_create, @function
sys_mem_create:
    /* a0 = file cap, a1 = memsz, a2 = shared, a3 = continuity, a4 = growth, a5 = file offset */
    li a7, SYS_MEM_CREATE
    ecall
    ret

    .global sys_mem_unmap
    .type   sys_mem_unmap, @function
sys_mem_unmap:
    li a7, SYS_MEM_UNMAP
    ecall
    ret

    .global sys_mem_query
    .type   sys_mem_query, @function
sys_mem_query:
    li a7, SYS_MEM_QUERY
    ecall
    ret

    .global sys_pcb_map
    .type   sys_pcb_map, @function
sys_pcb_map:
    li a7, SYS_PCB_MAP
    ecall
    ret

    .global sys_pcb_unmap
    .type   sys_pcb_unmap, @function
sys_pcb_unmap:
    li a7, SYS_PCB_UNMAP
    ecall
    ret

    .global sys_pcb_query_vspace
    .type   sys_pcb_query_vspace, @function
sys_pcb_query_vspace:
    li a7, SYS_PCB_QUERY_VSPACE
    ecall
    ret

    .global sys_vfs_opendir
    .type   sys_vfs_opendir, @function
sys_vfs_opendir:
    li a7, SYS_VFS_OPENDIR
    ecall
    ret

    .global sys_vfs_open
    .type   sys_vfs_open, @function
sys_vfs_open:
    li a7, SYS_VFS_OPEN
    ecall
    ret

    .global sys_vfs_mkfile
    .type   sys_vfs_mkfile, @function
sys_vfs_mkfile:
    li a7, SYS_VFS_MKFILE
    ecall
    ret

    .global sys_vfs_mkdir
    .type   sys_vfs_mkdir, @function
sys_vfs_mkdir:
    li a7, SYS_VFS_MKDIR
    ecall
    ret

    .global sys_vfs_unlink
    .type   sys_vfs_unlink, @function
sys_vfs_unlink:
    li a7, SYS_VFS_UNLINK
    ecall
    ret

    .global sys_vfs_rmdir
    .type   sys_vfs_rmdir, @function
sys_vfs_rmdir:
    li a7, SYS_VFS_RMDIR
    ecall
    ret

    .global sys_vfs_truncate
    .type   sys_vfs_truncate, @function
sys_vfs_truncate:
    li a7, SYS_VFS_TRUNCATE
    ecall
    ret

    .global sys_vfs_rename
    .type   sys_vfs_rename, @function
sys_vfs_rename:
    li a7, SYS_VFS_RENAME
    ecall
    ret

    .global sys_vfs_symlink
    .type   sys_vfs_symlink, @function
sys_vfs_symlink:
    li a7, SYS_VFS_SYMLINK
    ecall
    ret

    .global sys_vfs_link
    .type   sys_vfs_link, @function
sys_vfs_link:
    li a7, SYS_VFS_LINK
    ecall
    ret

    .global sys_vfs_stat
    .type   sys_vfs_stat, @function
sys_vfs_stat:
    li a7, SYS_VFS_STAT
    ecall
    ret

    .global sys_vfs_lstat
    .type   sys_vfs_lstat, @function
sys_vfs_lstat:
    li a7, SYS_VFS_LSTAT
    ecall
    ret

    .global sys_vfs_fstat
    .type   sys_vfs_fstat, @function
sys_vfs_fstat:
    li a7, SYS_VFS_FSTAT
    ecall
    ret

    .global sys_vfs_readlink
    .type   sys_vfs_readlink, @function
sys_vfs_readlink:
    li a7, SYS_VFS_READLINK
    ecall
    ret

    .global sys_vfs_read
    .type   sys_vfs_read, @function
sys_vfs_read:
    li a7, SYS_VFS_READ
    ecall
    ret

    .global sys_vfs_write
    .type   sys_vfs_write, @function
sys_vfs_write:
    li a7, SYS_VFS_WRITE
    ecall
    ret

    .global sys_vfs_size
    .type   sys_vfs_size, @function
sys_vfs_size:
    li a7, SYS_VFS_SIZE
    ecall
    ret

    .global sys_vfs_getdents
    .type   sys_vfs_getdents, @function
sys_vfs_getdents:
    li a7, SYS_VFS_GETDENTS
    ecall
    ret

    .global sys_vfs_sync
    .type   sys_vfs_sync, @function
sys_vfs_sync:
    li a7, SYS_VFS_SYNC
    ecall
    ret

    .global sys_vfs_fchownat
    .type   sys_vfs_fchownat, @function
sys_vfs_fchownat:
    li a7, SYS_VFS_FCHOWNAT
    ecall
    ret
