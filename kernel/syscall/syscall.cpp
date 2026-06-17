/**
 * @file syscall.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 系统调用
 * @version alpha-1.0.0
 * @date 2026-05-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <logger.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/callconv.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/callconv.h>
#endif
#include <sustcore/addr.h>
#include <sustcore/syscall.h>
#include <syscall/cap.h>
#include <syscall/endpoint.h>
#include <syscall/memory.h>
#include <syscall/notif.h>
#include <syscall/syscall.h>
#include <syscall/task.h>
#include <syscall/uaccess.h>
#include <syscall/vfs.h>
#include <task/scheduler.h>
#include <task/task_struct.h>
#include <task/wait.h>

#include <cassert>

namespace syscall {
    namespace {
        void log_getdents_probe(const char *stage,
                                util::nonnull<task::TCB *> tcb) {
            if (tcb->task == nullptr || tcb->task->tmm == nullptr) {
                return;
            }

            constexpr VirAddr kPcbCapAddr(0x0000000080005520ULL);
            auto vma_res = tcb->task->tmm->locate(kPcbCapAddr);
            if (!vma_res.has_value() || vma_res.value()->memory == nullptr) {
                return;
            }

            auto *vma      = vma_res.value().get();
            size_t mem_off = vma->mem_offset + (kPcbCapAddr - vma->varea.begin);
            CapIdx value   = cap::null;
            auto read_res  = vma->memory->read(mem_off, &value, sizeof(value));
            if (!read_res.has_value()) {
                return;
            }

            loggers::SYSCALL::INFO(
                "dispatch probe[%s]: pid=%lu vaddr=%p vma=[%p,%p) type=%d "
                "mem=%p mem_off=%lu value=%p",
                stage, tcb->task->pid, kPcbCapAddr.addr(),
                vma->varea.begin.addr(), vma->varea.end.addr(),
                static_cast<int>(vma->type), vma->memory, mem_off, value);
        }

        /**
         * @brief 生成布尔型 syscall 返回包.
         *
         * @param ok 操作结果.
         * @return RetPack 返回包.
         */
        [[nodiscard]]
        RetPack bool_ret(bool ok) noexcept {
            return RetPack{.processed = true,
                           .ret0      = static_cast<b64>(ok),
                           .ret1      = static_cast<b64>(ErrCode::SUCCESS)};
        }

        /**
         * @brief 将 Result<void> 转成 syscall 返回包.
         *
         * @param op 日志操作名.
         * @param res 调用结果.
         * @return RetPack 返回包.
         */
        [[nodiscard]]
        RetPack result_void_ret(const char *op,
                                const Result<void> &res) noexcept {
            if (!res.has_value()) {
                loggers::SYSCALL::ERROR("%s失败: err=%s", op,
                                        to_cstring(res.error()));
                return RetPack{.processed = true,
                               .ret0      = false,
                               .ret1      = static_cast<b64>(res.error())};
            }
            return bool_ret(true);
        }

        /**
         * @brief 将 Result<size_t> 转成 syscall 返回包.
         *
         * @param op 日志操作名.
         * @param res 调用结果.
         * @return RetPack 返回包.
         */
        [[nodiscard]]
        RetPack result_value_ret(const char *op,
                                 const Result<size_t> &res) noexcept {
            if (!res.has_value()) {
                loggers::SYSCALL::ERROR("%s失败: err=%s", op,
                                        to_cstring(res.error()));
                return RetPack{.processed = true,
                               .ret0      = 0,
                               .ret1      = static_cast<b64>(res.error())};
            }
            return RetPack{.processed = true,
                           .ret0      = res.value(),
                           .ret1      = static_cast<b64>(ErrCode::SUCCESS)};
        }

        /**
         * @brief 将 Result<bool> 转成 syscall 返回包.
         *
         * @param op 日志操作名.
         * @param res 调用结果.
         * @return RetPack 返回包.
         */
        [[nodiscard]]
        RetPack result_bool_ret(const char *op,
                                const Result<bool> &res) noexcept {
            if (!res.has_value()) {
                loggers::SYSCALL::ERROR("%s失败: err=%s", op,
                                        to_cstring(res.error()));
                return RetPack{.processed = true,
                               .ret0      = 0,
                               .ret1      = static_cast<b64>(res.error())};
            }
            return RetPack{.processed = true,
                           .ret0      = static_cast<b64>(res.value()),
                           .ret1      = static_cast<b64>(ErrCode::SUCCESS)};
        }

        /**
         * @brief 向串口写字符串.
         *
         * @param str 字符串代理.
         * @param len 输出长度.
         */
        void write_serial(const UString &str, size_t len) noexcept {
            sys_write_serial(str.kbuf(), len);
        }

        void begin_user_syscall(task::TCB *tcb, const ArgPack &args) noexcept {
            assert(tcb != nullptr);
            tcb->syscall_info.begin(args);
        }

        void handle_sync_user_syscall(task::TCB *tcb, Context *trap_context,
                                      const ArgPack &args) noexcept {
            assert(tcb != nullptr);
            assert(trap_context != nullptr);
            auto ret = dispatch_sync(util::nnullforce(tcb),
                                     util::nnullforce(trap_context), args);
            tcb->syscall_info.complete(ret);
            loggers::SYSCALL::DEBUG(
                "同步 syscall 立即完成: pid=%lu tid=%lu sysno=0x%lx",
                tcb->task != nullptr ? tcb->task->pid : 0, tcb->tid,
                args.syscall_number);
        }

    }  // namespace

    constexpr size_t MAX_SYSCALL_PATH = 256;

    const char *name_of(b64 sysno) {
        switch (sysno) {
            case SYS_WRITE_SERIAL:        return "SYS_WRITE_SERIAL";
            case SYS_CREATE_PROCESS:      return "SYS_CREATE_PROCESS";
            case SYS_PCB_KILL:            return "SYS_PCB_KILL";
            case SYS_YIELD:               return "SYS_YIELD";
            case SYS_LOG:                 return "SYS_LOG";
            case SYS_FORK:                return "SYS_FORK";
            case SYS_GETPID:              return "SYS_GETPID";
            case SYS_CREATE_THREAD:       return "SYS_CREATE_THREAD";
            case SYS_YIELD_THREAD:        return "SYS_YIELD_THREAD";
            case SYS_EXECVE:              return "SYS_EXECVE";
            case SYS_PCB_MAP:             return "SYS_PCB_MAP";
            case SYS_NOTIF_CREATE:        return "SYS_NOTIF_CREATE";
            case SYS_NOTIF_SIGNAL:        return "SYS_NOTIF_SIGNAL";
            case SYS_NOTIF_UNSIGNAL:      return "SYS_NOTIF_UNSIGNAL";
            case SYS_NOTIF_CHECK:         return "SYS_NOTIF_CHECK";
            case SYS_NOTIF_WAIT:          return "SYS_NOTIF_WAIT";
            case SYS_CAP_CLONE:           return "SYS_CAP_CLONE";
            case SYS_CAP_DOWNGRADE:       return "SYS_CAP_DOWNGRADE";
            case SYS_CAP_DERIVE:          return "SYS_CAP_DERIVE";
            case SYS_CAP_LOOKUP:          return "SYS_CAP_LOOKUP";
            case SYS_CAP_REMOVE:          return "SYS_CAP_REMOVE";
            case SYS_ENDPOINT_CREATE:     return "SYS_ENDPOINT_CREATE";
            case SYS_ENDPOINT_SEND:       return "SYS_ENDPOINT_SEND";
            case SYS_ENDPOINT_RECV:       return "SYS_ENDPOINT_RECV";
            case SYS_ENDPOINT_SEND_ASYNC: return "SYS_ENDPOINT_SEND_ASYNC";
            case SYS_ENDPOINT_RECV_ASYNC: return "SYS_ENDPOINT_RECV_ASYNC";
            case SYS_ENDPOINT_CALL:       return "SYS_ENDPOINT_CALL";
            case SYS_ENDPOINT_REPLY:      return "SYS_ENDPOINT_REPLY";
            case SYS_MEM_CREATE:          return "SYS_MEM_CREATE";
            case SYS_MEM_UNMAP:           return "SYS_MEM_UNMAP";
            case SYS_MEM_RESIZE:          return "SYS_MEM_RESIZE";
            case SYS_MEM_QUERY:           return "SYS_MEM_QUERY";
            case SYS_VFS_OPENDIR:         return "SYS_VFS_OPENDIR";
            case SYS_VFS_OPEN:            return "SYS_VFS_OPEN";
            case SYS_VFS_READ:            return "SYS_VFS_READ";
            case SYS_VFS_WRITE:           return "SYS_VFS_WRITE";
            case SYS_VFS_SIZE:            return "SYS_VFS_SIZE";
            case SYS_VFS_GETDENTS:        return "SYS_VFS_GETDENTS";
            case SYS_VFS_SYNC:            return "SYS_VFS_SYNC";
            case SYS_VFS_MKFILE:          return "SYS_VFS_MKFILE";
            case SYS_VFS_MKDIR:           return "SYS_VFS_MKDIR";
            default:                      return "UNKNOWN_SYSCALL";
        }
    }

    RetPack dispatch_sync(util::nonnull<task::TCB *> tcb,
                          util::nonnull<Context *> trap_context,
                          const ArgPack &args) {
        if (tcb->task == nullptr) {
            return RetPack{
                .processed = false,
                .ret0      = 0,
                .ret1      = static_cast<b64>(ErrCode::INVALID_PARAM),
            };
        }
        b64 capidx = args.args[0];
        b64 arg0   = args.args[1];
        b64 arg1   = args.args[2];
        b64 arg2   = args.args[3];
        b64 arg3   = args.args[4];
        b64 arg4   = args.args[5];
        b64 arg5   = args.args[6];
        b64 sysno  = args.syscall_number;

        RetPack ret{
            .processed = true,
            .ret0      = 0,
            .ret1      = static_cast<b64>(ErrCode::SUCCESS),
        };

        switch (sysno) {
            case SYS_WRITE_SERIAL: {
                UString str((VirAddr)arg0, arg1);
                write_serial(str, arg1);
                break;
            }
            case SYS_CREATE_PROCESS: {
                UBuffer caps_buf((VirAddr)arg1, arg2 * sizeof(CapIdx));
                auto sync_res = caps_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步进程能力列表", sync_res);
                    break;
                }
                b64 startup_blob_size = arg5;
                UBuffer startup_buf((VirAddr)arg4, startup_blob_size);
                if (startup_blob_size != 0) {
                    auto startup_sync_res = startup_buf.sync_from_user();
                    if (!startup_sync_res.has_value()) {
                        ret = result_void_ret("同步进程启动缓冲区",
                                              startup_sync_res);
                        break;
                    }
                }
                ret = result_value_ret(
                    "创建进程",
                    pcb_create_process(capidx, arg0, std::move(caps_buf), arg2,
                                       arg3,
                                       startup_blob_size == 0 ? nullptr
                                                              : &startup_buf,
                                       startup_blob_size));
                break;
            }
            case SYS_CREATE_THREAD: {
                ret = result_value_ret("创建线程",
                                       pcb_create_thread(capidx, VirAddr(arg0),
                                                         VirAddr(arg1), arg2));
                break;
            }
            case SYS_FORK: {
                UBuffer child_cap_buf((VirAddr)arg0, sizeof(CapIdx));
                auto sync_res = child_cap_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步fork返回缓冲区", sync_res);
                    break;
                }
                auto fork_res = pcb_fork(capidx, std::move(child_cap_buf));
                ret           = result_value_ret("fork", fork_res);
                break;
            }
            case SYS_EXECVE: {
                UBuffer reserved_buf((VirAddr)arg1, arg2 * sizeof(CapIdx));
                auto sync_res = reserved_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步execve保留能力列表", sync_res);
                    break;
                }
                b64 startup_blob_size = arg4;
                UBuffer startup_buf((VirAddr)arg3, startup_blob_size);
                if (startup_blob_size != 0) {
                    auto startup_sync_res = startup_buf.sync_from_user();
                    if (!startup_sync_res.has_value()) {
                        ret = result_void_ret("同步execve启动缓冲区",
                                              startup_sync_res);
                        break;
                    }
                }
                ret = result_bool_ret(
                    "execve",
                    pcb_execve(capidx, arg0, std::move(reserved_buf), arg2,
                               startup_blob_size == 0 ? nullptr : &startup_buf,
                               startup_blob_size));
                break;
            }
            case SYS_VFS_OPENDIR: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_value_ret("opendir",
                                       vfs_opendir(capidx, path, arg1));
                break;
            }
            case SYS_VFS_OPEN: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_value_ret("open", vfs_open(capidx, path, arg1));
                break;
            }
            case SYS_VFS_MKFILE: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret =
                    result_value_ret("mkfile", vfs_mkfile(capidx, path, arg1));
                break;
            }
            case SYS_VFS_MKDIR: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_value_ret("mkdir", vfs_mkdir(capidx, path, arg1));
                break;
            }
            case SYS_VFS_READ: {
                UBuffer buf((VirAddr)arg1, arg2);
                ret = result_value_ret(
                    "read", vfs_read(capidx, arg0, std::move(buf), arg2));
                break;
            }
            case SYS_VFS_WRITE: {
                UBuffer buf((VirAddr)arg1, arg2);
                auto sync_res = buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步write缓冲区", sync_res);
                    break;
                }
                ret = result_value_ret(
                    "write", vfs_write(capidx, arg0, std::move(buf), arg2));
                break;
            }
            case SYS_VFS_SIZE: {
                ret = result_value_ret("size", vfs_size(capidx));
                break;
            }
            case SYS_VFS_GETDENTS: {
                UBuffer buf((VirAddr)arg0, arg1);
                ret = result_value_ret(
                    "getdents",
                    vfs_getdents(capidx, std::move(buf), arg1, arg2));

                break;
            }
            case SYS_VFS_SYNC: {
                ret = result_bool_ret("sync", vfs_sync(capidx));
                break;
            }
            case SYS_PCB_KILL: {
                ret = result_bool_ret("pcb_kill",
                                      pcb_kill(capidx, static_cast<int>(arg0)));
                break;
            }
            case SYS_GETPID: {
                ret = result_value_ret("get_pid", get_pid(capidx));
                break;
            }
            case SYS_NOTIF_SIGNAL: {
                ret = result_bool_ret("设置notification",
                                      notification_signal(capidx, arg0, true));
                break;
            }
            case SYS_NOTIF_UNSIGNAL: {
                ret = result_bool_ret("取消notification",
                                      notification_signal(capidx, arg0, false));
                break;
            }
            case SYS_NOTIF_CHECK: {
                ret = result_bool_ret("查询notification",
                                      check_notification(capidx, arg0));
                break;
            }
            case SYS_NOTIF_WAIT: {
                ret = result_bool_ret("等待notification",
                                      wait_notification(capidx, arg0));
                break;
            }
            case SYS_NOTIF_CREATE: {
                ret =
                    result_value_ret("创建notification", notification_create());
                break;
            }
            case SYS_CAP_CLONE: {
                ret = result_value_ret("clone capability", cap_clone(capidx));
                break;
            }
            case SYS_CAP_DOWNGRADE: {
                ret = result_bool_ret("downgrade capability",
                                      cap_downgrade(capidx, arg0));
                break;
            }
            case SYS_CAP_DERIVE: {
                ret = result_value_ret("derive capability",
                                       cap_derive(capidx, arg0));
                break;
            }
            case SYS_CAP_LOOKUP: {
                UBuffer info_buf((VirAddr)arg0, sizeof(CapInfo));
                auto lookup_res = sys_cap_lookup(capidx, std::move(info_buf));
                ret = result_bool_ret("lookup capability", lookup_res);
                break;
            }
            case SYS_CAP_REMOVE: {
                ret = result_bool_ret("remove capability", cap_remove(capidx));
                break;
            }
            case SYS_ENDPOINT_CREATE: {
                ret = result_value_ret("创建endpoint", endpoint_create());
                break;
            }
            case SYS_ENDPOINT_SEND_ASYNC: {
                UBuffer packet_buf((VirAddr)arg0, sizeof(MsgPacket));
                auto sync_res = packet_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步异步发送消息包", sync_res);
                    break;
                }
                auto packet = *reinterpret_cast<MsgPacket *>(packet_buf.kbuf());
                ret         = result_bool_ret("异步发送endpoint消息",
                                              endpoint_send_async(capidx, packet));
                break;
            }
            case SYS_ENDPOINT_RECV_ASYNC: {
                UBuffer packet_buf((VirAddr)arg0, sizeof(MsgPacket));
                auto sync_res = packet_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步异步接收消息包", sync_res);
                    break;
                }
                auto packet = *reinterpret_cast<MsgPacket *>(packet_buf.kbuf());
                auto recv_res =
                    endpoint_recv_async(capidx, packet, std::move(packet_buf));
                ret = result_bool_ret("异步接收endpoint消息", recv_res);
                break;
            }
            case SYS_MEM_CREATE: {
                ret = result_value_ret(
                    "mem_create",
                    mem_create(arg0, arg1, arg2,
                               static_cast<cap::MemoryGrowth>(arg3)));
                break;
            }
            case SYS_PCB_MAP: {
                ret = result_bool_ret(
                    "pcb_map",
                    pcb_map(capidx, static_cast<CapIdx>(arg0), VirAddr(arg1),
                            static_cast<PageMan::RWX>(arg2),
                            static_cast<cap::MemoryGrowth>(arg3)));
                break;
            }
            case SYS_MEM_UNMAP: {
                ret = result_bool_ret("mem_unmap",
                                      mem_unmap(capidx, VirAddr(arg0)));
                break;
            }
            case SYS_MEM_RESIZE: {
                ret = result_bool_ret("mem_resize", mem_resize(capidx, arg0));
                break;
            }
            case SYS_MEM_QUERY: {
                UBuffer out_buf((VirAddr)arg0, sizeof(MemQueryRet));
                auto query_res = mem_query(capidx, std::move(out_buf));
                ret            = result_void_ret("mem_query", query_res);
                break;
            }
            case SYS_ENDPOINT_REPLY: {
                UBuffer reply_buf((VirAddr)arg0, sizeof(MsgPacket));
                auto sync_res = reply_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步reply消息包", sync_res);
                    break;
                }
                auto reply_packet =
                    *reinterpret_cast<MsgPacket *>(reply_buf.kbuf());
                ret = result_void_ret("endpoint_reply",
                                      endpoint_reply(capidx, reply_packet));
                break;
            }
            case SYS_ENDPOINT_SEND: {
                UBuffer packet_buf((VirAddr)arg0, sizeof(MsgPacket));
                auto sync_res = packet_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步发送消息包", sync_res);
                    break;
                }
                auto packet = *reinterpret_cast<MsgPacket *>(packet_buf.kbuf());
                ret         = result_void_ret("同步发送endpoint消息",
                                              endpoint_send_sync(capidx, packet));
                break;
            }
            case SYS_ENDPOINT_RECV: {
                UBuffer packet_buf((VirAddr)arg0, sizeof(MsgPacket));
                auto sync_res = packet_buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步接收消息包", sync_res);
                    break;
                }
                auto packet = *reinterpret_cast<MsgPacket *>(packet_buf.kbuf());
                ret         = result_void_ret(
                    "接收endpoint消息",
                    endpoint_recv_sync(capidx, packet, std::move(packet_buf)));
                break;
            }
            case SYS_ENDPOINT_CALL: {
                UBuffer send_buf((VirAddr)arg0, sizeof(MsgPacket));
                auto send_sync_res = send_buf.sync_from_user();
                if (!send_sync_res.has_value()) {
                    ret = result_void_ret("同步call发送消息包", send_sync_res);
                    break;
                }
                auto send_packet =
                    *reinterpret_cast<MsgPacket *>(send_buf.kbuf());
                UBuffer reply_buf((VirAddr)arg1, sizeof(MsgPacket));
                auto reply_sync_res = reply_buf.sync_from_user();
                if (!reply_sync_res.has_value()) {
                    ret = result_void_ret("同步call回复消息包", reply_sync_res);
                    break;
                }
                auto reply_packet =
                    *reinterpret_cast<MsgPacket *>(reply_buf.kbuf());
                ret = result_void_ret(
                    "endpoint_call",
                    endpoint_call(capidx, send_packet, reply_packet,
                                  std::move(reply_buf)));
                break;
            }
            default: {
                ret.processed = false;
                ret.ret0      = 0;
                ret.ret1      = static_cast<b64>(ErrCode::NOT_SUPPORTED);
                loggers::SYSCALL::ERROR("未知的系统调用号: %d", sysno);
                break;
            }
        }
        write_ret(*trap_context, ret);
        return ret;
    }

    void handle_user_ecall(util::nonnull<task::TCB *> tcb,
                           util::nonnull<Context *> trap_context,
                           const ArgPack &args) noexcept {
        Interrupt::sti();
        begin_user_syscall(tcb, args);
        handle_sync_user_syscall(tcb.get(), trap_context.get(), args);
    }
}  // namespace syscall
