/**
 * @file rpc.c
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 远程过程调用
 * @version alpha-1.0.0
 * @date 2025-11-25
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <kmod/syscall.h>
#include <string.h>

/**
 * @brief 远程过程调用
 *
 * @param pid 目标进程ID
 * @param fid 函数ID
 * @param args 参数指针
 * @param arg_size 参数大小
 * @param ret_buf 返回值缓冲区指针
 * @param ret_size 返回值缓冲区大小
 */
void rpc_call(Capability pid, int fid, const void *args, size_t arg_size,
              void *ret_buf, size_t ret_size) {
    size_t sharemem_sz = arg_size > ret_size ? arg_size : ret_size;

    // 首先分配共享内存
    Capability shmid = makesharedmem(sharemem_sz);
    if (shmid.raw == 0) {
        return;
    }

    // 将参数写入该共享内存, 返回值也应同样写入该共享内存(将参数覆写)
    void *shmaddr = getsharedmem(shmid);
    memcpy(shmaddr, args, arg_size);

    // 将参数所在共享内存共享给目标进程
    sharemem_with(pid, shmid);

    // 将RPC调用标志, fid, 共享内存编号与参数大小写入参数包
    const char *rpc_msg = (const char[]){
        (char)RPC_CALL_MSG,
        (char)(fid & 0xFF),
        (char)((shmid.raw >> 0) & 0xFF),
        (char)((shmid.raw >> 8) & 0xFF),
        (char)((shmid.raw >> 16) & 0xFF),
        (char)((shmid.raw >> 24) & 0xFF),
        (char)((arg_size >> 0) & 0xFF),
        (char)((arg_size >> 8) & 0xFF),
        (char)((arg_size >> 16) & 0xFF),
        (char)((arg_size >> 24) & 0xFF),
    };

    // 发送消息给目标进程
    send_message(pid, rpc_msg, sizeof(rpc_msg));

    // 等待目标进程处理完毕
    wait_process(pid);

    // 读取返回值
    memcpy(ret_buf, shmaddr, ret_size);

    // 释放共享内存
    freesharedmem(shmid);
}