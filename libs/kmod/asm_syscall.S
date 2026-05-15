    .text
    .global kwrites
    .type   kwrites, @function
kwrites:
    /* a0 = str, a1 = len */
    /* 将字符串写入串口 */
    /* 我们调用系统调用 sys_write_serial : 0xFFC0'0001 */
    li a7, 0xFFC00001
    ecall
    ret

    .global sys_create_process
    .type   sys_create_process, @function
    .global create_process
    .type   create_process, @function
sys_create_process:
create_process:
    /* a0 = path, a1 = initial caps, a2 = initial caps size */
    /* 创建并唤醒一个用户进程 */
    li a7, 0xFFC00002
    ecall
    ret

    .global sys_grow_vma
    .type sys_grow_vma, @function
sys_grow_vma:
    /* a0 = heap_base, a1 = new_brk */
    mv a2, a1
    mv a1, a0
    li a7, 0xFFC00003
    ecall
    /* kernel returns a0 = area.begin, a1 = area.end */
    mv a0, a1
    ret

    .global sys_wait_notification
    .type sys_wait_notification, @function
sys_wait_notification:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF0008
    ecall
    ret

    .global sys_signal_notification
    .type sys_signal_notification, @function
sys_signal_notification:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF000A
    ecall
    ret

    .global sys_unsignal_notification
    .type sys_unsignal_notification, @function
sys_unsignal_notification:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF000B
    ecall
    ret

    .global sys_check_notification
    .type sys_check_notification, @function
sys_check_notification:
    /* a0 = cap slot, a1 = signal idx */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF000C
    ecall
    ret

    .global sys_cap_clone
    .type sys_cap_clone, @function
sys_cap_clone:
    /* a0 = src cap slot, a1 = target cap slot */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF000E
    ecall
    ret

    .global sys_cap_downgrade
    .type sys_cap_downgrade, @function
sys_cap_downgrade:
    /* a0 = cap slot, a1 = new permission bits */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF000F
    ecall
    ret

    .global sys_cap_derive
    .type sys_cap_derive, @function
sys_cap_derive:
    /* a0 = src cap slot, a1 = target cap slot, a2 = new permission bits */
    mv a6, a0
    mv a0, a1
    mv a1, a2
    li a7, 0xFFFF0010
    ecall
    ret

    .global lookup_cap
    .type lookup_cap, @function
lookup_cap:
    /* a0 = cap slot, a1 = CapInfo out ptr */
    mv a6, a0
    mv a0, a1
    li a7, 0xFFFF0011
    ecall
    ret

    .global sys_getpid
    .type sys_getpid, @function
sys_getpid:
    /* a0 = pcb cap slot */
    mv a6, a0
    li a7, 0xFFFF0005
    ecall
    ret

    .global sys_create_notification
    .type sys_create_notification, @function
sys_create_notification:
    /* a0 = target cap slot */
    mv a6, a0
    li a7, 0xFFFF000D
    ecall
    ret
