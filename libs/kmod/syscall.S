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
