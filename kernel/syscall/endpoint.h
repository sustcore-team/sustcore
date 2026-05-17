/**
 * @file endpoint.h
 * @brief Endpoint syscalls
 */

#pragma once

#include <sustcore/addr.h>
#include <sustcore/capability.h>
#include <syscall/syscall.h>

#include <cstddef>

namespace syscall {
    /**
     * @brief 创建一个endpoint对象, 并将其cap放入调用者的CSpace中
     * 
     * @param capidx 调用者CSpace中用于存放新cap的索引
     * @return true 创建成功
     * @return false 创建失败
     */
    bool endpoint_create(CapIdx capidx);
    /**
     * @brief 向指定endpoint发送消息
     * 
     * @param endpoint 目标endpoint的cap索引
     * @param packet 用户态MsgPacket地址.
     * @param blocking 是否阻塞发送, 如果为true且目标endpoint没有准备好接收消息, 则调用将被阻塞直到可以发送; 如果为false则调用将立即返回失败
     * @return true 消息发送成功
     * @return false 消息发送失败
     */
    bool endpoint_send(CapIdx endpoint, VirAddr packet, bool blocking);
    /**
     * @brief 从端点处接收信息
     *
     * @param endpoint 端点cap索引
     * @param packet 用户态MsgPacket地址, 用于写回接收到的消息.
     * @return RetPack 返回结果, 包含是否成功、是否需要defer、错误码等信息
     */
    util::cotask<RetPack> endpoint_recv_sync(CapIdx endpoint, VirAddr packet);
    /**
     * @brief 从端点处接收信息 (异步版本)
     * 
     * @param endpoint 端点cap索引
     * @param packet 用户态MsgPacket地址, 用于写回接收到的消息.
     * @return true 消息接收成功
     * @return false 消息接收失败
     */
    bool endpoint_recv_async(CapIdx endpoint, VirAddr packet);

    /**
     * @brief 发起一次同步endpoint调用.
     *
     * 内核会创建ReplyObject, 向sendmsg追加REPLIER|MIGRATE_ONCE权限的reply cap,
     * 发送后阻塞等待CALLER端收到回复, 最后清理调用方reply cap.
     */
    util::cotask<RetPack> endpoint_call(CapIdx endpoint, VirAddr sendmsg,
                                        VirAddr replymsg);
    /**
     * @brief 向ReplyObject写入回复并移除当前CSpace中的reply cap.
     */
    bool endpoint_reply(CapIdx reply_cap, VirAddr replymsg);
}  // namespace syscall
