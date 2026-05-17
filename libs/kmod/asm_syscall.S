#include <sustcore/syscall.h>

    .text
    .global sys_write_serial
    .type   sys_write_serial, @function
sys_write_serial:
    /* a0 = str, a1 = len */
    /* 将字符串写入串口 */
    /* 我们调用系统调用 sys_write_serial */
    li a7, SYS_WRITE_SERIAL
    ecall
    ret

    .global sys_exit
    .type sys_exit, @function
sys_exit:
    li a0, 0
    la t0, __pcb_cap
    ld a6, 0(t0)
    li a7, SYS_PCB_KILL
    ecall
1:
    j 1b

    .global sys_pcb_kill
    .type sys_pcb_kill, @function
sys_pcb_kill:
    /* a0 = pcb cap slot, a1 = exit code */
    mv a6, a0
    mv a0, a1
    li a7, SYS_PCB_KILL
    ecall
    ret

    .global sys_create_process
    .type   sys_create_process, @function
sys_create_process:
    /* a0 = path, a1 = initial caps, a2 = initial caps size, a3 = sched class */
    /* 创建并唤醒一个用户进程 */
    li a7, SYS_CREATE_PROCESS
    ecall
    ret

    .global sys_create_thread
    .type   sys_create_thread, @function
sys_create_thread:
    /* a0 = entry, a1 = stack_addr, a2 = stack_size */
    li a7, SYS_CREATE_THREAD
    ecall
    ret

    .global sys_fork
    .type   sys_fork, @function
sys_fork:
    li a7, SYS_FORK
    ecall
    ret

    .global sys_execve
    .type   sys_execve, @function
    .global execve
    .type   execve, @function
sys_execve:
execve:
    /* a0 = path, a1 = reserved caps, a2 = reserved caps size */
    li a7, SYS_EXECVE
    ecall
    ret

    .global sys_notif_wait
    .type sys_notif_wait, @function
sys_notif_wait:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, SYS_NOTIF_WAIT
    ecall
    ret

    .global sys_notif_signal
    .type sys_notif_signal, @function
sys_notif_signal:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, SYS_NOTIF_SIGNAL
    ecall
    ret

    .global sys_notif_unsignal
    .type sys_notif_unsignal, @function
sys_notif_unsignal:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, SYS_NOTIF_UNSIGNAL
    ecall
    ret

    .global sys_notif_check
    .type sys_notif_check, @function
sys_notif_check:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, SYS_NOTIF_CHECK
    ecall
    ret

    .global sys_cap_clone
    .type sys_cap_clone, @function
sys_cap_clone:
    /* a0 = src cap slot, a1 = target cap slot */
    mv a6, a0
    mv a0, a1
    li a7, SYS_CAP_CLONE
    ecall
    ret

    .global sys_cap_downgrade
    .type sys_cap_downgrade, @function
sys_cap_downgrade:
    /* a0 = cap slot, a1 = new permission bits */
    mv a6, a0
    mv a0, a1
    li a7, SYS_CAP_DOWNGRADE
    ecall
    ret

    .global sys_cap_derive
    .type sys_cap_derive, @function
sys_cap_derive:
    /* a0 = src cap slot, a1 = target cap slot, a2 = new permission bits */
    mv a6, a0
    mv a0, a1
    mv a1, a2
    li a7, SYS_CAP_DERIVE
    ecall
    ret

    .global sys_cap_lookup
    .type sys_cap_lookup, @function
sys_cap_lookup:
    /* a0 = cap slot, a1 = CapInfo out ptr */
    mv a6, a0
    mv a0, a1
    li a7, SYS_CAP_LOOKUP
    ecall
    ret

    .global sys_cap_remove
    .type sys_cap_remove, @function
sys_cap_remove:
    /* a0 = cap slot */
    mv a6, a0
    li a7, SYS_CAP_REMOVE
    ecall
    ret

    .global sys_getpid
    .type sys_getpid, @function
sys_getpid:
    /* a0 = pcb cap slot */
    mv a6, a0
    li a7, SYS_GETPID
    ecall
    ret

    .global sys_notif_create
    .type sys_notif_create, @function
sys_notif_create:
    /* a0 = target cap slot */
    mv a6, a0
    li a7, SYS_NOTIF_CREATE
    ecall
    ret

    .global sys_endpoint_create
    .type sys_endpoint_create, @function
sys_endpoint_create:
    /* a0 = target cap slot */
    mv a6, a0
    li a7, SYS_ENDPOINT_CREATE
    ecall
    ret

    .global sys_endpoint_send
    .type sys_endpoint_send, @function
sys_endpoint_send:
    /* a0 = endpoint, a1 = MsgPacket* */
    mv a6, a0
    mv a0, a1
    li a7, SYS_ENDPOINT_SEND
    ecall
    ret

    .global sys_endpoint_recv
    .type sys_endpoint_recv, @function
sys_endpoint_recv:
    /* a0 = endpoint, a1 = MsgPacket* */
    mv a6, a0
    mv a0, a1
    li a7, SYS_ENDPOINT_RECV
    ecall
    ret

    .global sys_endpoint_send_async
    .type sys_endpoint_send_async, @function
sys_endpoint_send_async:
    /* a0 = endpoint, a1 = MsgPacket* */
    mv a6, a0
    mv a0, a1
    li a7, SYS_ENDPOINT_SEND_ASYNC
    ecall
    ret

    .global sys_endpoint_recv_async
    .type sys_endpoint_recv_async, @function
sys_endpoint_recv_async:
    /* a0 = endpoint, a1 = MsgPacket* */
    mv a6, a0
    mv a0, a1
    li a7, SYS_ENDPOINT_RECV_ASYNC
    ecall
    ret

    .global endpoint_call
    .type endpoint_call, @function
endpoint_call:
    /* a0 = endpoint, a1 = send MsgPacket*, a2 = reply MsgPacket* */
    mv a6, a0
    mv a0, a1
    mv a1, a2
    li a7, SYS_ENDPOINT_CALL
    ecall
    ret

    .global endpoint_reply
    .type endpoint_reply, @function
endpoint_reply:
    /* a0 = reply cap, a1 = reply MsgPacket* */
    mv a6, a0
    mv a0, a1
    li a7, SYS_ENDPOINT_REPLY
    ecall
    ret

    .global sys_mem_create
    .type sys_mem_create, @function
sys_mem_create:
    /* a0 = cap slot, a1 = memsz, a2 = shared, a3 = continuity, a4 = growth */
    mv a6, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    li a7, SYS_MEM_CREATE
    ecall
    ret

    .global sys_mem_map
    .type sys_mem_map, @function
sys_mem_map:
    /* a0 = cap slot, a1 = vaddr, a2 = rwx, a3 = growth */
    mv a4, a3
    mv a3, a2
    mv a2, a1
    mv a1, a0
    la t0, __pcb_cap
    ld a6, 0(t0)
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    li a7, SYS_PCB_MAP
    ecall
    ret

    .global sys_pcb_map
    .type sys_pcb_map, @function
sys_pcb_map:
    /* a0 = pcb cap slot, a1 = mem cap slot, a2 = vaddr, a3 = rwx, a4 = growth */
    mv a6, a0
    mv a0, a1
    mv a1, a2
    mv a2, a3
    mv a3, a4
    li a7, SYS_PCB_MAP
    ecall
    ret

    .global sys_mem_unmap
    .type sys_mem_unmap, @function
sys_mem_unmap:
    /* a0 = cap slot, a1 = vaddr */
    mv a6, a0
    mv a0, a1
    li a7, SYS_MEM_UNMAP
    ecall
    ret

    .global sys_mem_resize
    .type sys_mem_resize, @function
sys_mem_resize:
    /* a0 = cap slot, a1 = newsz */
    mv a6, a0
    mv a0, a1
    li a7, SYS_MEM_RESIZE
    ecall
    ret

    .global sys_mem_query
    .type sys_mem_query, @function
sys_mem_query:
    /* a0 = cap slot */
    mv a6, a0
    li a7, SYS_MEM_QUERY
    ecall
    ret
