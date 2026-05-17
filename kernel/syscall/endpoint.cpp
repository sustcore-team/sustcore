/**
 * @file endpoint.cpp
 * @brief Endpoint syscalls
 */

#include <cap/cholder.h>
#include <logger.h>
#include <object/endpoint.h>
#include <object/perm.h>
#include <sus/coroutine.h>
#include <sus/nonnull.h>
#include <sus/raii.h>
#include <sustcore/capability.h>
#include <sustcore/errcode.h>
#include <syscall/endpoint.h>
#include <syscall/uaccess.h>
#include <task/scheduler.h>
#include <task/wait.h>

#include <cassert>
#include <coroutine>
#include <cstring>

namespace syscall {
    namespace {
        /**
         * @brief MsgPacket在内核中的地址视图.
         *
         * 用户态MsgPacket本身只在系统调用入口处复制一次; 之后内核使用该结构保存
         * 各字段对应的用户虚拟地址, 并按需通过UBuffer读写实际数据.
         */
        struct KMsgPacket {
            /// 用户消息缓冲区地址.
            VirAddr msgbuf;
            /// 用户态size_t*, 发送时提供消息大小, 接收时写回实际大小.
            VirAddr msgsz;
            /// 用户cap列表缓冲区地址.
            VirAddr caplist;
            /// 用户态size_t*, 发送时提供cap数量, 接收时写回实际数量.
            VirAddr capsz;
        };

        /**
         * @brief 已从用户态MsgPacket读取并校验过的待发送消息.
         */
        struct PreparedMsg {
            /// 内核中的消息正文副本.
            char msgbuf[MAX_MSG_SIZE]{};
            /// 消息正文大小.
            size_t msgsz = 0;
            /// 用户传入的cap索引副本.
            CapIdx capidxs[MAX_MSG_CAPS]{};
            /// cap索引解析得到的cap对象指针.
            cap::Capability *caps[MAX_MSG_CAPS]{};
            /// 与caps一一对应, 标记该cap是否按迁移语义发送.
            bool migrate_caps[MAX_MSG_CAPS]{};
            /// 附带cap数量.
            size_t capsz = 0;
        };

        /**
         * @brief endpoint_call过程中创建的一对Reply Capability槽位.
         */
        struct ReplySlots {
            /// caller持有的CALLER权限reply cap.
            CapIdx caller = cap::null;
            /// 临时用于发送给replier的REPLIER|MIGRATE_ONCE权限reply cap.
            CapIdx replier = cap::null;
        };

        /**
         * @brief 查找并包装当前CSpace中的EndpointObject.
         */
        Result<cap::EndpointObject> endpoint_object(CapIdx capidx) {
            auto cap_res = cap::CHolder::lookup(capidx);
            propagate(cap_res);
            auto *cap = cap_res.value();
            if (cap->payload()->type_id() != PayloadType::ENDPOINT) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            return cap::EndpointObject(util::nnullforce(cap));
        }

        /**
         * @brief 查找并包装当前CSpace中的ReplyObject.
         */
        Result<cap::ReplyObject> reply_object(CapIdx capidx) {
            auto cap_res = cap::CHolder::lookup(capidx);
            propagate(cap_res);
            auto *cap = cap_res.value();
            if (cap->payload()->type_id() != PayloadType::REPLY) {
                unexpect_return(ErrCode::TYPE_NOT_MATCHED);
            }
            return cap::ReplyObject(util::nnullforce(cap));
        }

        /**
         * @brief 返回当前线程所属进程的pid.
         */
        pid_t current_pid() {
            auto *tcb = schd::Scheduler::inst().current_tcb();
            if (tcb == nullptr || tcb->task == nullptr) {
                return 0;
            }
            return tcb->task->pid;
        }

        /**
         * @brief 从用户态读取MsgPacket描述符.
         */
        Result<KMsgPacket> read_packet(VirAddr packet) {
            if (!packet.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }

            UBuffer packet_buf(packet, sizeof(MsgPacket));
            packet_buf.sync_from_user();
            auto *upacket = reinterpret_cast<MsgPacket *>(packet_buf.kbuf());

            KMsgPacket kpacket{
                .msgbuf  = VirAddr(upacket->msgbuf),
                .msgsz   = VirAddr(upacket->msgsz),
                .caplist = VirAddr(upacket->caplist),
                .capsz   = VirAddr(upacket->capsz),
            };
            if (!kpacket.msgsz.nonnull() || !kpacket.capsz.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }
            return kpacket;
        }

        /**
         * @brief 从用户态读取一个size_t.
         */
        Result<size_t> read_size(VirAddr uaddr) {
            if (!uaddr.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }
            UBuffer buf(uaddr, sizeof(size_t));
            buf.sync_from_user();
            return *reinterpret_cast<size_t *>(buf.kbuf());
        }

        /**
         * @brief 向用户态写回一个size_t.
         */
        Result<void> write_size(VirAddr uaddr, size_t value) {
            if (!uaddr.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }
            UBuffer buf(uaddr, sizeof(size_t));
            memcpy(buf.kbuf(), &value, sizeof(size_t));
            buf.sync_to_user();
            void_return();
        }

        /**
         * @brief 根据KMsgPacket读取消息正文和cap列表, 形成可发送消息.
         */
        Result<void> prepare_msg(const KMsgPacket &packet, PreparedMsg &out) {
            auto msgsz_res = read_size(packet.msgsz);
            propagate(msgsz_res);
            auto capsz_res = read_size(packet.capsz);
            propagate(capsz_res);

            out.msgsz = msgsz_res.value();
            out.capsz = capsz_res.value();
            if (out.msgsz > MAX_MSG_SIZE || out.capsz > MAX_MSG_CAPS) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            if (out.msgsz != 0 && !packet.msgbuf.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (out.capsz != 0 && !packet.caplist.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }

            if (out.msgsz != 0) {
                UBuffer msg_buf(packet.msgbuf, out.msgsz);
                msg_buf.sync_from_user();
                memcpy(out.msgbuf, msg_buf.kbuf(), out.msgsz);
            }

            if (out.capsz != 0) {
                UBuffer caps_buf(packet.caplist, out.capsz * sizeof(CapIdx));
                caps_buf.sync_from_user();
                memcpy(out.capidxs, caps_buf.kbuf(),
                       out.capsz * sizeof(CapIdx));

                for (size_t i = 0; i < out.capsz; ++i) {
                    auto cap_res = cap::CHolder::lookup(out.capidxs[i]);
                    propagate(cap_res);
                    out.caps[i] = cap_res.value();
                }
            }

            void_return();
        }

        /**
         * @brief 向PreparedMsg追加一个cap.
         *
         * @param migrate 为true时, EndpointObject::send会按迁移语义传递该cap.
         */
        Result<void> append_cap(PreparedMsg &msg, CapIdx capidx,
                                cap::Capability *cap, bool migrate) {
            if (msg.capsz >= MAX_MSG_CAPS || cap == nullptr) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            msg.capidxs[msg.capsz]      = capidx;
            msg.caps[msg.capsz]         = cap;
            msg.migrate_caps[msg.capsz] = migrate;
            msg.capsz++;
            void_return();
        }

        /**
         * @brief 将收到的消息附带cap插入目标CHolder空闲槽位.
         *
         * 若中途失败, 已插入的cap会被回滚移除.
         */
        Result<void> insert_received_caps(cap::CHolder *holder,
                                          cap::EndpointMessage *msg,
                                          CapIdx *out_caps) {
            if (holder == nullptr || msg == nullptr || out_caps == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            CapIdx inserted[MAX_MSG_CAPS]{};
            size_t inserted_count = 0;
            util::Guard cleanup([&]() {
                for (size_t i = 0; i < inserted_count; ++i) {
                    auto remove_res = holder->internal_remove(inserted[i]);
                    assert(remove_res.has_value());
                }
            });

            for (size_t i = 0; i < msg->capsz; ++i) {
                auto slot_res = holder->internal_insert_to_free(
                    msg->caps[i]->payload(), msg->caps[i]->perm());
                propagate(slot_res);
                inserted[inserted_count++] = slot_res.value();
                out_caps[i]                = slot_res.value();
            }

            cleanup.release();
            void_return();
        }

        /**
         * @brief 将EndpointMessage写回用户态MsgPacket指定的缓冲区.
         */
        Result<void> write_received_msg(cap::CHolder *holder,
                                        cap::EndpointMessage *msg,
                                        const KMsgPacket &packet) {
            if (holder == nullptr || msg == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (!packet.msgsz.nonnull() || !packet.capsz.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }
            if (msg->msgsz != 0 && !packet.msgbuf.nonnull()) {
                unexpect_return(ErrCode::NULLPTR);
            }

            CapIdx out_caps[MAX_MSG_CAPS]{};
            if (msg->capsz != 0) {
                if (!packet.caplist.nonnull()) {
                    unexpect_return(ErrCode::NULLPTR);
                }
                auto insert_res = insert_received_caps(holder, msg, out_caps);
                propagate(insert_res);
            }

            if (msg->msgsz != 0) {
                UBuffer out_msg(packet.msgbuf, msg->msgsz);
                memcpy(out_msg.kbuf(), msg->msgbuf, msg->msgsz);
                out_msg.sync_to_user();
            }

            auto write_msgsz_res = write_size(packet.msgsz, msg->msgsz);
            propagate(write_msgsz_res);
            auto write_capsz_res = write_size(packet.capsz, msg->capsz);
            propagate(write_capsz_res);

            if (msg->capsz != 0) {
                UBuffer out_caplist(packet.caplist,
                                    msg->capsz * sizeof(CapIdx));
                memcpy(out_caplist.kbuf(), out_caps,
                       msg->capsz * sizeof(CapIdx));
                out_caplist.sync_to_user();
            }

            void_return();
        }

        /**
         * @brief 获取当前任务的CHolder.
         */
        Result<cap::CHolder *> current_holder() {
            return cap::CHolder::current();
        }

        /**
         * @brief 构造endpoint recv/call类协程系统调用的bool返回包.
         */
        syscall::RetPack recv_ret(bool ok) {
            return syscall::RetPack{true, static_cast<b64>(ok), 0};
        }

        /**
         * @brief 尝试从ReplyObject取出消息并写回用户态.
         *
         * @return true表示成功写回; false表示当前尚无reply消息.
         */
        Result<bool> try_write_reply(cap::CHolder *holder,
                                     cap::ReplyObject &reply_obj,
                                     VirAddr replymsg) {
            auto recv_res = reply_obj.recv_async();
            propagate(recv_res);
            cap::EndpointMessage *msg = recv_res.value();
            if (msg == nullptr) {
                return false;
            }

            util::Guard msg_guard([&]() { delete msg; });
            auto reply_packet_res = read_packet(replymsg);
            propagate(reply_packet_res);
            auto write_res =
                write_received_msg(holder, msg, reply_packet_res.value());
            propagate(write_res);
            return true;
        }

        /**
         * @brief 读取并校验endpoint_call请求消息.
         */
        Result<PreparedMsg> prepare_call_msg(VirAddr sendmsg) {
            auto send_packet_res = read_packet(sendmsg);
            propagate(send_packet_res);

            PreparedMsg msg{};
            auto prep_res = prepare_msg(send_packet_res.value(), msg);
            propagate(prep_res);
            if (msg.capsz >= MAX_MSG_CAPS) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }
            return msg;
        }

        /**
         * @brief 为endpoint_call创建并降权caller/replier两端Reply Capability.
         *
         * 失败时会回滚已经插入的cap; 成功后由调用者负责清理两个槽位.
         */
        Result<ReplySlots> create_reply_slots(cap::CHolder *holder) {
            if (holder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }

            auto *reply_payload = new cap::ReplyPayload();
            if (reply_payload == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }

            auto caller_slot_res =
                holder->internal_insert_to_free(reply_payload, perm::allperm());
            if (!caller_slot_res.has_value()) {
                delete reply_payload;
                propagate_return(caller_slot_res);
            }
            CapIdx caller_slot = caller_slot_res.value();
            auto caller_guard  = util::Guard([&]() {
                auto remove_res = holder->internal_remove(caller_slot);
                assert(remove_res.has_value());
            });

            auto replier_slot_res = holder->internal_lookup_freeslot();
            propagate(replier_slot_res);
            CapIdx replier_slot = replier_slot_res.value();
            auto clone_res = holder->internal_clone(replier_slot, caller_slot);
            propagate(clone_res);
            auto replier_guard = util::Guard([&]() {
                auto remove_res = holder->internal_remove(replier_slot);
                assert(remove_res.has_value());
            });

            auto replier_downgrade_res = holder->internal_downgrade(
                replier_slot, perm::reply::REPLIER | perm::basic::MIGRATE_ONCE);
            propagate(replier_downgrade_res);

            auto caller_downgrade_res =
                holder->internal_downgrade(caller_slot, perm::reply::CALLER);
            propagate(caller_downgrade_res);

            replier_guard.release();
            caller_guard.release();
            return ReplySlots{caller_slot, replier_slot};
        }

        /**
         * @brief 将replier端Reply Capability追加到endpoint_call请求消息中.
         */
        Result<void> append_replier_cap(cap::CHolder *holder, PreparedMsg &msg,
                                        CapIdx replier_slot) {
            if (holder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }

            auto replier_cap_res = holder->internal_lookup(replier_slot);
            propagate(replier_cap_res);
            return append_cap(msg, replier_slot, replier_cap_res.value(),
                              true);
        }

        /**
         * @brief 发送endpoint_call请求消息.
         */
        Result<void> send_call_request(CapIdx endpoint, PreparedMsg &msg) {
            auto send_res =
                endpoint_object(endpoint).and_then([&](cap::EndpointObject obj) {
                    return obj.send(current_pid(), msg.msgbuf, msg.msgsz,
                                    msg.caps, msg.capsz, true,
                                    msg.migrate_caps);
                });
            propagate(send_res);
            if (!send_res.value()) {
                unexpect_return(ErrCode::FAILURE);
            }
            void_return();
        }

        /**
         * @brief 读取并准备endpoint_reply回复消息.
         */
        Result<PreparedMsg> prepare_reply_msg(VirAddr replymsg) {
            auto packet_res = read_packet(replymsg);
            propagate(packet_res);

            PreparedMsg msg{};
            auto prep_res = prepare_msg(packet_res.value(), msg);
            propagate(prep_res);
            return msg;
        }

        /**
         * @brief 将PreparedMsg写入指定Reply Capability.
         */
        Result<void> send_reply_to_cap(CapIdx reply_cap, PreparedMsg &msg) {
            auto send_res =
                reply_object(reply_cap).and_then([&](cap::ReplyObject obj) {
                    return obj.send_reply(current_pid(), msg.msgbuf, msg.msgsz,
                                          msg.caps, msg.capsz);
                });
            propagate(send_res);
            if (!send_res.value()) {
                unexpect_return(ErrCode::FAILURE);
            }
            void_return();
        }

        /**
         * @brief 从当前CSpace移除已使用的一次性Reply Capability.
         */
        Result<void> remove_reply_cap(CapIdx reply_cap) {
            return cap::CHolder::remove(reply_cap);
        }
    }  // namespace

    bool endpoint_create(CapIdx capidx) {
        auto create_res = cap::CHolder::create<cap::EndpointPayload>(capidx);
        if (!create_res.has_value()) {
            loggers::SYSCALL::ERROR("创建endpoint失败: err=%d",
                                    create_res.error());
            return false;
        }
        return true;
    }

    bool endpoint_send(CapIdx endpoint, VirAddr packet, bool blocking) {
        auto packet_res = read_packet(packet);
        if (!packet_res.has_value()) {
            loggers::SYSCALL::ERROR("发送endpoint消息失败: packet err=%d",
                                    packet_res.error());
            return false;
        }

        PreparedMsg msg{};
        auto prep_res = prepare_msg(packet_res.value(), msg);
        if (!prep_res.has_value()) {
            loggers::SYSCALL::ERROR("发送endpoint消息失败: prepare err=%d",
                                    prep_res.error());
            return false;
        }

        auto send_res =
            endpoint_object(endpoint).and_then([&](cap::EndpointObject obj) {
                return obj.send(current_pid(), msg.msgbuf, msg.msgsz, msg.caps,
                                msg.capsz, blocking);
            });
        if (!send_res.has_value()) {
            loggers::SYSCALL::ERROR("发送endpoint消息失败: err=%d",
                                    send_res.error());
            return false;
        }
        return send_res.value();
    }

    bool endpoint_recv_async(CapIdx endpoint, VirAddr packet) {
        auto packet_res = read_packet(packet);
        if (!packet_res.has_value()) {
            return false;
        }

        auto holder_res = current_holder();
        if (!holder_res.has_value()) {
            loggers::SYSCALL::ERROR(
                "接收endpoint消息失败: 当前CSpace不可用 err=%d",
                holder_res.error());
            return false;
        }

        auto recv_res = endpoint_object(endpoint).and_then(
            [](cap::EndpointObject obj) { return obj.recv_async(); });
        if (!recv_res.has_value()) {
            loggers::SYSCALL::ERROR("接收endpoint消息失败: err=%d",
                                    recv_res.error());
            return false;
        }

        cap::EndpointMessage *msg = recv_res.value();
        if (msg == nullptr) {
            return false;
        }
        util::Guard msg_guard([&]() { delete msg; });

        auto write_res =
            write_received_msg(holder_res.value(), msg, packet_res.value());
        if (!write_res.has_value()) {
            loggers::SYSCALL::ERROR("接收endpoint消息失败: 写回失败 err=%d",
                                    write_res.error());
            return false;
        }

        return true;
    }

    util::cotask<syscall::RetPack> endpoint_recv_sync(CapIdx endpoint,
                                                 VirAddr packet) {
        auto packet_res = read_packet(packet);
        if (!packet_res.has_value()) {
            co_return recv_ret(false);
        }

        auto holder_res = current_holder();
        if (!holder_res.has_value()) {
            loggers::SYSCALL::ERROR(
                "接收endpoint消息失败: 当前CSpace不可用 err=%d",
                holder_res.error());
            co_return recv_ret(false);
        }

        auto endpoint_res = endpoint_object(endpoint);
        if (!endpoint_res.has_value()) {
            loggers::SYSCALL::ERROR("接收endpoint消息失败: err=%d",
                                    endpoint_res.error());
            co_return recv_ret(false);
        }

        cap::EndpointObject endpoint_obj = endpoint_res.value();
        auto awaiter_res                 = endpoint_obj.recv_sync();
        if (!awaiter_res.has_value()) {
            loggers::SYSCALL::ERROR("接收endpoint消息失败: err=%d",
                                    awaiter_res.error());
            co_return recv_ret(false);
        }

        auto awaiter = awaiter_res.value();
        co_await awaiter;

        auto recv_res = endpoint_obj.recv_async();
        bool ok       = false;
        if (!recv_res.has_value()) {
            loggers::SYSCALL::ERROR("接收endpoint消息失败: err=%d",
                                    recv_res.error());
        } else {
            cap::EndpointMessage *msg = recv_res.value();
            if (msg != nullptr) {
                util::Guard msg_guard([&]() { delete msg; });

                auto write_res = write_received_msg(holder_res.value(), msg,
                                                    packet_res.value());
                ok             = write_res.has_value();
                if (!write_res.has_value()) {
                    loggers::SYSCALL::ERROR(
                        "接收endpoint消息失败: 写回失败 err=%d",
                        write_res.error());
                }
            }
        }
        co_return recv_ret(ok);
    }

    util::cotask<syscall::RetPack> endpoint_call(CapIdx endpoint,
                                                 VirAddr sendmsg,
                                                 VirAddr replymsg) {
        // 1) 准备请求消息
        auto msg_res = prepare_call_msg(sendmsg);
        if (!msg_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call准备请求失败: err=%s",
                                    to_cstring(msg_res.error()));
            co_return recv_ret(false);
        }
        PreparedMsg msg = msg_res.value();

        // 2) 获得双方CSpace并向其中插入 Replier/Caller Reply Capability
        auto holder_res = current_holder();
        if (!holder_res.has_value()) {
            loggers::SYSCALL::ERROR(
                "endpoint_call失败: 当前CSpace不可用 err=%d",
                holder_res.error());
            co_return recv_ret(false);
        }
        cap::CHolder *holder = holder_res.value();

        auto slots_res = create_reply_slots(holder);
        if (!slots_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call创建reply cap失败: err=%s",
                                    to_cstring(slots_res.error()));
            co_return recv_ret(false);
        }
        ReplySlots slots = slots_res.value();

        // guard, 确保reply cap的清理
        auto caller_guard = util::Guard([&]() {
            auto remove_res = holder->internal_remove(slots.caller);
            assert(remove_res.has_value());
        });
        auto replier_guard = util::Guard([&]() {
            auto remove_res = holder->internal_remove(slots.replier);
            assert(remove_res.has_value());
        });

        // 3) 附加一次性replier cap并发送请求
        auto append_res = append_replier_cap(holder, msg, slots.replier);
        if (!append_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call追加reply cap失败: err=%s",
                                    to_cstring(append_res.error()));
            co_return recv_ret(false);
        }

        auto send_res = send_call_request(endpoint, msg);
        if (!send_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call发送请求失败: err=%s",
                                    to_cstring(send_res.error()));
            co_return recv_ret(false);
        }

        // 4) 在请求发送成功后立即移除replier cap, 无论后续如何都不再需要它了.
        auto remove_replier_res = holder->internal_remove(slots.replier);
        if (!remove_replier_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call移除临时replier失败: err=%d",
                                    remove_replier_res.error());
            co_return recv_ret(false);
        }
        replier_guard.release();

        // 5) 等待reply消息
        auto reply_res = reply_object(slots.caller);
        if (!reply_res.has_value()) {
            loggers::SYSCALL::ERROR(
                "endpoint_call lookup caller reply失败: err=%s",
                to_cstring(reply_res.error()));
            co_return recv_ret(false);
        }
        cap::ReplyObject reply_obj = reply_res.value();

        // 试图直接从reply对象取出消息并写回
        auto immediate_reply_res = try_write_reply(holder, reply_obj, replymsg);
        if (!immediate_reply_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call同步写回reply失败: err=%s",
                                    to_cstring(immediate_reply_res.error()));
            co_return recv_ret(false);
        }
        if (immediate_reply_res.value()) {
            co_return recv_ret(true);
        }

        // 否则, 等待reply消息到来
        auto awaiter_res = reply_obj.recv_sync();
        if (!awaiter_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call等待reply失败: err=%d",
                                    awaiter_res.error());
            co_return recv_ret(false);
        }

        auto awaiter = awaiter_res.value();
        co_await awaiter;

        // 最后再尝试一次写回reply消息
        auto waited_reply_res = try_write_reply(holder, reply_obj, replymsg);
        if (!waited_reply_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_call写回reply失败: err=%d",
                                    waited_reply_res.error());
            co_return recv_ret(false);
        }
        if (!waited_reply_res.value()) {
            loggers::SYSCALL::ERROR("endpoint_call唤醒后没有reply消息");
            co_return recv_ret(false);
        }
        co_return recv_ret(true);
    }

    bool endpoint_reply(CapIdx reply_cap, VirAddr replymsg) {
        // 1) 从调用者提供的消息包中获得 Reply Object
        auto prep_res = prepare_reply_msg(replymsg);
        if (!prep_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_reply prepare失败: err=%s",
                                    to_cstring(prep_res.error()));
            return false;
        }
        PreparedMsg msg = prep_res.value();

        // 2) 将回应写入一次性Reply Capability, 发送给调用者
        auto send_res = send_reply_to_cap(reply_cap, msg);
        if (!send_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_reply发送失败: err=%s",
                                    to_cstring(send_res.error()));
            return false;
        }

        // 3) 删除本方的reply cap, 完成一次性语义
        auto remove_res = remove_reply_cap(reply_cap);
        if (!remove_res.has_value()) {
            loggers::SYSCALL::ERROR("endpoint_reply删除reply cap失败: err=%d",
                                    remove_res.error());
            return false;
        }
        return true;
    }
}  // namespace syscall
