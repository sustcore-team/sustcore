/**
 * @file syscall.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用接口
 * @version alpha-1.0.0
 * @date 2025-11-25
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#pragma once

#include <stddef.h>
#include <kmod/capability.h>

/**
 * @brief 退出当前进程
 * 
 * @param code 退出码
 */
void exit(int code);

/**
 * @brief 唤醒指定进程
 * 
 * @param pid 进程ID(实际上是进程能力)
 */
void wakeup_process(Capability pid);

/**
 * @brief 等待指定进程
 * 
 * @param pid 进程ID(实际上是进程能力)
 */
void wait_process(Capability pid);

/**
 * @brief 使当前进程休眠指定的毫秒数
 * 
 * @param ms 毫秒数
 */
void sleep(unsigned int ms);

/**
 * @brief 创建共享内存
 * 
 * @param size 共享内存大小
 * @return Capability 共享内存能力
 */
Capability makesharedmem(size_t size);

/**
 * @brief 共享内存给指定进程
 * 
 * @param pid 目标进程ID(实际上是进程能力)
 * @param shmid 共享内存能力
 */
void sharemem_with(Capability pid, Capability shmid);

/**
 * @brief 获取共享内存
 * 
 * @param cap 共享内存能力
 * @return void* 指向共享内存的指针
 */
void *getsharedmem(Capability cap);

/**
 * @brief 释放共享内存
 * 
 * @param cap 共享内存能力
 */
void freesharedmem(Capability cap);

/**
 * @brief 申请物理内存
 * 
 * @param size 物理内存大小
 * @return Capability 物理内存能力
 */
Capability require_phymem(size_t size);

/**
 * @brief 获得物理内存地址
 * 
 * @param cap 物理内存能力
 * @return void* 指向物理内存的指针
 */
void *getphyaddr(Capability cap);

/**
 * @brief 映射物理内存到虚拟地址空间
 * 
 * @param cap 物理内存能力
 * @return void* 指向映射后的虚拟内存地址
 */
void *mapphymem(Capability cap);

/**
 * @brief 释放物理内存
 * 
 * @param cap 物理内存能力
 */
void freephymem(Capability cap);

/**
 * @brief 发送消息给指定进程
 * 
 * @param pid 进程ID(实际上是进程能力)
 * @param msg 消息指针
 * @param size 消息大小
 */
void send_message(Capability pid, const void *msg, size_t size);

#define RPC_CALL_MSG (0xFF)

/**
 * @brief 远程过程调用
 * 
 * @param pid        目标进程ID(实际上是进程能力)
 * @param fid        函数ID
 * @param args       参数指针
 * @param arg_size   参数大小
 * @param ret_buf    返回值缓冲区指针
 * @param ret_size   返回值缓冲区大小
 */
void rpc_call(Capability pid, int fid, const void *args, size_t arg_size, void *ret_buf, size_t ret_size);