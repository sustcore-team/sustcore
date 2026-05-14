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

    .global create_process
    .type   create_process, @function
create_process:
    /* a0 = path */
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
