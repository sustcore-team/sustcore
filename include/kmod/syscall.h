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

#include <kmod/capability.h>
#include <stddef.h>

/**
 * @brief 退出当前进程
 *
 * @param code 退出码
 */
void exit(int code);

/**
 * @brief 让出CPU，主动让调度器调度其他进程/线程
 * @param thread 线程能力. 如果是INVALID_CAP_PTR, 则表示让出当前进程
 */
void yield(CapPtr thread);

/**
 * @brief 获得当前进程的PID
 * 
 * @return 当前进程的PID
 */
int get_current_pid(void);

/**
 * @brief 创建当前进程的副本
 * 
 * @return int 新进程的PID, 子进程返回0
 */
int fork(void);

/**
 * @brief 唤醒指定进程
 *
 * @param pid 进程ID(实际上是进程能力)
 */
void wakeup_process(CapPtr pid);

/**
 * @brief 等待指定进程
 *
 * @param pid 进程ID(实际上是进程能力)
 */
void wait_process(CapPtr pid);

/**
 * @brief 等待通知
 *
 * @param thread 线程能力. 如果是INVALID_CAP_PTR, 则表示以进程级别让出CPU
 * @param notif_cap 通知能力
 * @param wait_bitmap 等待的通知位图
 */
void wait_notifications(CapPtr thread, CapPtr notif_cap, qword *wait_bitmap);

/**
 * @brief 等待通知
 *
 * @param thread 线程能力. 如果是INVALID_CAP_PTR, 则表示以进程级别让出CPU
 * @param notif_cap 通知能力
 * @param notification_id 等待的通知ID
 */
void wait_notification(CapPtr thread, CapPtr notif_cap, int notification_id);

/**
 * @brief 设置通知位
 * 
 * @param notif_cap 通知能力
 * @param notification_id 通知ID
 */
void notification_set(CapPtr notif_cap, int notification_id);

/**
 * @brief 重置通知位
 * 
 * @param notif_cap 通知能力
 * @param notification_id 通知ID
 */
void notification_reset(CapPtr notif_cap, int notification_id);

/**
 * @brief 检查通知位
 * 
 * @param notif_cap 通知能力
 * @param notification_id 通知ID
 * @return true 通知已设置
 * @return false 通知未设置
 */
bool check_notification(CapPtr notif_cap, int notification_id);

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
 * @return CapPtr 共享内存能力
 */
CapPtr makesharedmem(size_t size);

/**
 * @brief 共享内存给指定进程
 *
 * @param pid 目标进程ID(实际上是进程能力)
 * @param shmid 共享内存能力
 */
void sharemem_with(CapPtr pid, CapPtr shmid);

/**
 * @brief 获取共享内存
 *
 * @param cap 共享内存能力
 * @return void* 指向共享内存的指针
 */
void *getsharedmem(CapPtr cap);

/**
 * @brief 释放共享内存
 *
 * @param cap 共享内存能力
 */
void freesharedmem(CapPtr cap);

/**
 * @brief 申请物理内存
 *
 * @param size 物理内存大小
 * @return CapPtr 物理内存能力
 */
CapPtr require_phymem(size_t size);

/**
 * @brief 获得内存物理地址
 *
 * 你必须要被允许获得该内存的物理地址
 *
 * @param cap 内存能力
 * @return void* 内存的物理地址
 */
void *getphyaddr(CapPtr cap);

/**
 * @brief 映射内存到虚拟地址空间
 *
 * @param cap 内存能力
 * @return void* 指向映射后的虚拟内存地址
 */
void *mapmem(CapPtr cap);

/**
 * @brief 释放内存
 *
 * @param cap 内存能力
 */
void freemem(CapPtr cap);

/**
 * @brief 发送消息给指定进程
 *
 * @param pid 进程ID(实际上是进程能力)
 * @param msg 消息指针
 * @param size 消息大小
 */
void send_message(CapPtr pid, const void *msg, size_t size);

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
void rpc_call(CapPtr pid, int fid, const void *args, size_t arg_size,
              void *ret_buf, size_t ret_size);

/**
 * @brief 创建线程
 * 
 * @param entrypoint 线程入口点
 * @param priority 线程优先级
 * @return CapPtr 线程能力指针
 */
CapPtr create_thread(void *entrypoint, int priority);