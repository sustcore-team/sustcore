/**
 * @file endpoint.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief IPC端点对象
 * @version alpha-1.0.0
 * @date 2026-05-17
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/int.h>
#include <env.h>
#include <guard.h>
#include <logger.h>
#include <mem/vma.h>
#include <object/endpoint.h>
#include <object/perm.h>
#include <task/scheduler.h>
#include <task/wait.h>

#include <cassert>
#include <cstring>

namespace cap {
    namespace {
        /**
         * @brief 检测消息是否有效.
         *
         * @param msg 消息视图
         */
        Result<void> check_msg_valid(const EndpointMsgView &msg) {
            if (msg.msgsz > MAX_MSG_SIZE || msg.capsz > MAX_MSG_CAPS) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (msg.msgsz != 0 && msg.msgbuf == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (msg.capsz != 0 && msg.capidxs == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            void_return();
        }

        /**
         * @brief 从 MsgView 构建一个 EndpointMessage.
         *
         * @param sender_pid 发送者PID, 用于填充EndpointMessage的sender_pid字段
         * @param view 消息视图, 包含消息内容和cap索引列表
         * @return Result<util::owner<EndpointMessage *>> 构建结果,
         * 成功时包含一个EndpointMessage的owner指针; 失败时包含错误码
         */
        Result<util::owner<EndpointMessage *>> build_endpoint_message(
            pid_t sender_pid, const EndpointMsgView &view) {
            propagate(check_msg_valid(view));

            // 创建一个 msg 对象
            util::owner<EndpointMessage *> msg =
                util::owner(new EndpointMessage());
            auto msg_guard = delete_guard(msg);

            if (msg == nullptr) {
                unexpect_return(ErrCode::ALLOCATION_FAILED);
            }

            // 初始化msg
            msg->sender_pid = sender_pid;
            msg->msgsz      = view.msgsz;
            msg->capsz      = view.capsz;
            if (view.msgsz != 0) {
                memcpy(msg->msgbuf, view.msgbuf, view.msgsz);
            }
            for (size_t i = 0; i < view.capsz; ++i) {
                msg->capidxs[i] = view.capidxs[i];
            }

            msg_guard.release();
            return msg;
        }

        // to make sure if the given message is still in the endpoint's message
        // queue used for send_sync to detect if the message has been consumed
        bool message_is_queued(EndpointPayload *payload, EndpointMessage *msg) {
            if (payload == nullptr || msg == nullptr) {
                return false;
            }
            for (auto &message : payload->messages) {
                if (&message == msg) {
                    return true;
                }
            }
            return false;
        }

        PendingEndpointSend *find_pending_send(
            EndpointPayload *payload, EndpointMessage *msg) {
            if (payload == nullptr || msg == nullptr) {
                return nullptr;
            }
            for (auto &pending : payload->pending_sends) {
                if (pending.message == msg) {
                    return &pending;
                }
            }
            return nullptr;
        }

        PendingEndpointRecv *pop_pending_recv(EndpointPayload *payload) {
            if (payload == nullptr || payload->pending_recvs.empty()) {
                return nullptr;
            }
            auto *pending = &payload->pending_recvs.front();
            payload->pending_recvs.pop_front();
            return pending;
        }

        PendingReplyRecv *pop_pending_recv(ReplyPayload *payload) {
            if (payload == nullptr || payload->pending_recvs.empty()) {
                return nullptr;
            }
            auto *pending = &payload->pending_recvs.front();
            payload->pending_recvs.pop_front();
            return pending;
        }
    }  // namespace

    EndpointPayload::EndpointPayload()
        : messages{},
          pending_sends{},
          pending_recvs{} {}

    EndpointPayload::~EndpointPayload() {
        while (!pending_sends.empty()) {
            auto *pending = &pending_sends.front();
            pending_sends.pop_front();
            if (pending->message != nullptr) {
                if (message_is_queued(this, pending->message)) {
                    messages.remove(*pending->message);
                }
                delete pending->message;
                pending->message = nullptr;
            }
            pending->promise.set_cancel_callback({});
            delete pending;
        }
        while (!messages.empty()) {
            EndpointMessage *msg = &messages.front();
            messages.pop_front();
            delete msg;
        }
        while (!pending_recvs.empty()) {
            auto *pending = &pending_recvs.front();
            pending_recvs.pop_front();
            pending->promise.set_cancel_callback({});
            delete pending;
        }
    }

    ReplyPayload::ReplyPayload() : message(nullptr), pending_recvs{} {}

    ReplyPayload::~ReplyPayload() {
        delete message;
        message = nullptr;
        while (!pending_recvs.empty()) {
            auto *pending = &pending_recvs.front();
            pending_recvs.pop_front();
            pending->promise.set_cancel_callback({});
            delete pending;
        }
    }

    Result<wait::Future<void>> EndpointObject::send(
        pid_t sender_pid, const EndpointMsgView &view) {
        propagate(check_msg_valid(view));
        if (!imply(perm::endpoint::WRITE)) {
            loggers::CAPABILITY::ERROR("Endpoint WRITE权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }
        if (view.capsz != 0 && !imply(perm::endpoint::GRANT)) {
            loggers::CAPABILITY::ERROR("Endpoint GRANT权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto msg_res = build_endpoint_message(sender_pid, view);
        propagate(msg_res);
        util::owner<EndpointMessage *> msg = msg_res.value();

        auto pending = util::owner(new PendingEndpointSend());
        if (pending == nullptr) {
            unexpect_return(ErrCode::ALLOCATION_FAILED);
        }
        pending->message = msg.get();
        msg = util::owner<EndpointMessage *>(nullptr);
        auto future      = pending->promise.future();

        InterruptGuard guard;
        guard.enter();
        auto *pending_ptr = pending.get();
        pending_ptr->promise.set_cancel_callback([payload = _obj,
                                                  pending_ptr]() -> Result<void> {
            InterruptGuard cancel_guard;
            cancel_guard.enter();
            if (payload == nullptr || pending_ptr == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            if (find_pending_send(payload, pending_ptr->message) != pending_ptr) {
                void_return();
            }
            if (pending_ptr->message != nullptr &&
                message_is_queued(payload, pending_ptr->message))
            {
                payload->messages.remove(*pending_ptr->message);
            }
            payload->pending_sends.remove(*pending_ptr);
            delete pending_ptr->message;
            pending_ptr->message = nullptr;
            delete pending_ptr;
            void_return();
        });

        _obj->pending_sends.push_back(*pending_ptr);
        auto *pending_recv = pop_pending_recv(_obj);
        if (pending_recv != nullptr) {
            auto set_recv_res = pending_recv->promise.set_value(pending_ptr->message);
            if (!set_recv_res.has_value()) {
                _obj->pending_recvs.push_back(*pending_recv);
                propagate_return(set_recv_res);
            }
            auto complete_res = pending_ptr->promise.set_value();
            if (!complete_res.has_value()) {
                loggers::CAPABILITY::WARN("完成 endpoint send future 失败: %s",
                                          to_cstring(complete_res.error()));
            }
            pending_ptr->message = nullptr;
            delete pending_recv;
            _obj->pending_sends.remove(*pending_ptr);
            delete pending_ptr;
        } else {
            _obj->messages.push_back(*pending_ptr->message);
        }
        pending = util::owner<PendingEndpointSend *>(nullptr);
        return future;
    }

    Result<wait::Future<EndpointMessage *>> EndpointObject::recv() {
        if (!imply(perm::endpoint::READ)) {
            loggers::CAPABILITY::ERROR("Endpoint READ权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto pending = util::owner(new PendingEndpointRecv());
        if (pending == nullptr) {
            unexpect_return(ErrCode::ALLOCATION_FAILED);
        }
        auto future = pending->promise.future();

        InterruptGuard guard;
        guard.enter();
        if (!_obj->messages.empty()) {
            EndpointMessage *msg = &_obj->messages.front();
            _obj->messages.pop_front();
            auto *pending_send = find_pending_send(_obj, msg);
            if (pending_send != nullptr) {
                _obj->pending_sends.remove(*pending_send);
                auto complete_res = pending_send->promise.set_value();
                if (!complete_res.has_value()) {
                    loggers::CAPABILITY::WARN("完成 endpoint send future 失败: %s",
                                              to_cstring(complete_res.error()));
                }
                pending_send->message = nullptr;
                delete pending_send;
            }
            auto set_res = pending->promise.set_value(msg);
            propagate(set_res);
            return future;
        }
        auto *pending_ptr = pending.get();
        pending_ptr->promise.set_cancel_callback([payload = _obj,
                                                  pending_ptr]() -> Result<void> {
            InterruptGuard cancel_guard;
            cancel_guard.enter();
            if (payload == nullptr || pending_ptr == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            payload->pending_recvs.remove(*pending_ptr);
            delete pending_ptr;
            void_return();
        });
        _obj->pending_recvs.push_back(*pending_ptr);
        pending = util::owner<PendingEndpointRecv *>(nullptr);
        return future;
    }

    Result<bool> ReplyObject::send_reply(pid_t sender_pid,
                                         const EndpointMsgView &view) {
        // replier端必须持有REPLIER权限; reply payload只允许写入一条消息.
        propagate(check_msg_valid(view));
        if (!imply(perm::reply::REPLIER)) {
            loggers::CAPABILITY::ERROR("Reply REPLIER权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        // 构造回复消息. 与Endpoint消息相同, cap只记录发送方CapIdx,
        // 具体CLONE/MIGRATE/MIGRATE_ONCE处理留到接收方写回时完成.
        util::owner<EndpointMessage *> msg = util::owner(new EndpointMessage());
        auto msg_guard                     = delete_guard(msg);
        if (msg == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        msg->sender_pid = sender_pid;
        msg->msgsz      = view.msgsz;
        msg->capsz      = view.capsz;
        if (view.msgsz != 0) {
            memcpy(msg->msgbuf, view.msgbuf, view.msgsz);
        }
        for (size_t i = 0; i < view.capsz; ++i) {
            msg->capidxs[i] = view.capidxs[i];
        }

        // 在临界区内检查单条reply槽是否已经被占用, 并安装回复消息.
        {
            InterruptGuard guard;
            guard.enter();

            if (_obj->message != nullptr) {
                return false;
            }

            auto *pending_recv = pop_pending_recv(_obj);
            if (pending_recv != nullptr) {
                auto set_res = pending_recv->promise.set_value(msg.get());
                if (!set_res.has_value()) {
                    delete pending_recv;
                    propagate_return(set_res);
                }
                msg_guard.release();
                delete pending_recv;
                return true;
            }

            _obj->message = msg.get();
            msg_guard.release();
        }
        return true;
    }

    Result<void> ReplyObject::reply(pid_t sender_pid, CHolder *holder,
                                    CapIdx reply_cap,
                                    const EndpointMsgView &msg) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        // reply是send_reply加一次性cap清理的薄包装; send_reply返回false表示
        // 该ReplyPayload已经有未取走的回复.
        auto send_res = send_reply(sender_pid, msg);
        propagate(send_res);
        if (!send_res.value()) {
            unexpect_return(ErrCode::FAILURE);
        }

        // 服务端本方的replier cap只允许使用一次, 成功写入回复后立即移除.
        auto remove_res = holder->remove(reply_cap);
        propagate(remove_res);
        void_return();
    }

    Result<wait::Future<EndpointMessage *>> ReplyObject::recv() {
        if (!imply(perm::reply::CALLER)) {
            loggers::CAPABILITY::ERROR("Reply CALLER权限不足");
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        auto pending = util::owner(new PendingReplyRecv());
        if (pending == nullptr) {
            unexpect_return(ErrCode::ALLOCATION_FAILED);
        }
        auto future = pending->promise.future();

        InterruptGuard guard;
        guard.enter();
        if (_obj->message != nullptr) {
            EndpointMessage *msg = _obj->message;
            _obj->message        = nullptr;
            auto set_res         = pending->promise.set_value(msg);
            propagate(set_res);
            return future;
        }
        auto *pending_ptr = pending.get();
        pending_ptr->promise.set_cancel_callback([payload = _obj,
                                                  pending_ptr]() -> Result<void> {
            InterruptGuard cancel_guard;
            cancel_guard.enter();
            if (payload == nullptr || pending_ptr == nullptr) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            payload->pending_recvs.remove(*pending_ptr);
            delete pending_ptr;
            void_return();
        });
        _obj->pending_recvs.push_back(*pending_ptr);
        pending = util::owner<PendingReplyRecv *>(nullptr);
        return future;
    }
}  // namespace cap
