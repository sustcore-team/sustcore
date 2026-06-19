/**
 * @file endpoint.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief IPC端点对象
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <fwd.h>
#include <cap/capability.h>
#include <object/perm.h>
#include <sus/coroutine.h>
#include <sus/list.h>
#include <sustcore/capability.h>
#include <sustcore/msg.h>
#include <task/task_struct.h>
#include <task/wait.h>

#include <cstddef>

namespace cap {
    /**
     * @brief Endpoint发送侧消息视图, 不持有缓冲区或cap数组.
     */
    struct EndpointMsgView {
        const char *msgbuf = nullptr;
        size_t msgsz       = 0;
        CapIdx *capidxs    = nullptr;
        size_t capsz       = 0;
    };

    /**
     * @brief 端点消息
     *
     */
    struct EndpointMessage {
        pid_t sender_pid = 0;
        char msgbuf[MAX_MSG_SIZE]{};
        size_t msgsz = 0;
        CapIdx capidxs[MAX_MSG_CAPS]{};
        size_t capsz = 0;
        util::ListHead<EndpointMessage> list_head{};
    };

    struct PendingEndpointSend {
        util::ListHead<PendingEndpointSend> list_head{};
        EndpointMessage *message = nullptr;
        wait::Promise<void> promise{};
    };

    struct PendingEndpointRecv {
        util::ListHead<PendingEndpointRecv> list_head{};
        wait::Promise<EndpointMessage *> promise{};
    };

    struct PendingReplyRecv {
        util::ListHead<PendingReplyRecv> list_head{};
        wait::Promise<EndpointMessage *> promise{};
    };

    /**
     * @brief Endpoint对象的payload.
     *
     * 包含消息列表与接收/发送等待队列
     *
     */
    struct EndpointPayload : public _PayloadHelper<PayloadType::ENDPOINT> {
        util::IntrusiveList<EndpointMessage, &EndpointMessage::list_head>
            messages = {};
        util::IntrusiveList<PendingEndpointSend, &PendingEndpointSend::list_head>
            pending_sends = {};
        util::IntrusiveList<PendingEndpointRecv, &PendingEndpointRecv::list_head>
            pending_recvs = {};

        EndpointPayload();
        ~EndpointPayload() override;
    };

    /**
     * @brief 一次性调用回复对象的payload.
     *
     * ReplyPayload最多持有一条EndpointMessage, 用于endpoint_call的调用方
     * 阻塞等待endpoint_reply写入结果.
     */
    struct ReplyPayload : public _PayloadHelper<PayloadType::REPLY> {
        EndpointMessage *message = nullptr;
        util::IntrusiveList<PendingReplyRecv, &PendingReplyRecv::list_head>
            pending_recvs = {};

        ReplyPayload();
        ~ReplyPayload() override;
    };

    class EndpointObject : public CapObj<EndpointPayload> {
    public:
        explicit EndpointObject(util::nonnull<Capability *> cap)
            : CapObj<EndpointPayload>(cap) {}

        /**
         * @brief 向endpoint发送消息, 返回异步完成句柄.
         *
         * Future就绪表示该消息已经被接收方消费.
         */
        Result<wait::Future<void>> send(
            pid_t sender_pid, const EndpointMsgView &msg);
        /**
         * @brief 发起一次接收请求, 返回异步结果句柄.
         *
         * Future就绪后可读取接收到的消息指针.
         */
        Result<wait::Future<EndpointMessage *>> recv();
    };

    /**
     * @brief Reply Capability对象.
     *
     * 拥有REPLIER权限的一端可写入一次回复; 拥有CALLER权限的一端可阻塞读取.
     */
    class ReplyObject : public CapObj<ReplyPayload> {
    public:
        explicit ReplyObject(util::nonnull<Capability *> cap)
            : CapObj<ReplyPayload>(cap) {}

        /**
         * @brief 写入一次回复消息.
         *
         * 调用者必须持有REPLIER权限. ReplyObject已有消息时返回false.
         */
        Result<bool> send_reply(pid_t sender_pid, const EndpointMsgView &msg);
        /**
         * @brief 发送一次回复并从 holder 中移除本方一次性 reply cap.
         */
        Result<void> reply(pid_t sender_pid, CHolder *holder, CapIdx reply_cap,
                           const EndpointMsgView &msg);
        /**
         * @brief 发起一次回复读取请求, 返回异步结果句柄.
         */
        Result<wait::Future<EndpointMessage *>> recv();
    };
}  // namespace cap
