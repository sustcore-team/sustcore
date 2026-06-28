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
#include <object/task.h>
#include <env.h>
#include <sustcore/addr.h>
#include <sustcore/execve.h>
#include <sustcore/syscall.h>
#include <syscall/cap.h>
#include <syscall/endpoint.h>
#include <syscall/memory.h>
#include <syscall/notif.h>
#include <syscall/pipe.h>
#include <syscall/shutdown.h>
#include <syscall/syscall.h>
#include <syscall/task.h>
#include <syscall/uaccess.h>
#include <syscall/vfs.h>
#include <task/scheduler.h>
#include <task/task_struct.h>
#include <task/wait.h>

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

namespace syscall {
    namespace {
        constexpr size_t MAX_STARTUP_ITEMS         = 64;
        constexpr size_t MAX_STARTUP_STRING        = 256;
        constexpr size_t MAX_BOOTSTRAP_RECORD_SIZE = 4096;

        template <typename T>
        [[nodiscard]]
        Result<std::vector<T>> copy_terminated_values(VirAddr uaddr,
                                                      T terminator) {
            if (!uaddr.nonnull()) {
                return std::vector<T>{};
            }

            std::vector<T> values{};
            for (size_t i = 0; i < MAX_STARTUP_ITEMS; ++i) {
                UBuffer slot_buf(uaddr + i * sizeof(T), sizeof(T));
                auto sync_res = slot_buf.sync_from_user();
                propagate(sync_res);
                T value{};
                memcpy(&value, slot_buf.kbuf(), sizeof(T));
                if (value == terminator) {
                    return values;
                }
                values.push_back(value);
            }
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        [[nodiscard]]
        Result<std::vector<std::string>> copy_terminated_strings(VirAddr uaddr) {
            auto ptrs_res =
                copy_terminated_values<addr_t>(uaddr, static_cast<addr_t>(0));
            propagate(ptrs_res);

            std::vector<std::string> strings{};
            for (addr_t ptr : ptrs_res.value()) {
                UString text(VirAddr(ptr), MAX_STARTUP_STRING);
                auto sync_res = text.sync_from_user();
                propagate(sync_res);
                strings.emplace_back(text.kbuf(), text.len());
            }
            return strings;
        }

        [[nodiscard]]
        Result<std::vector<TaskSpec::BootstrapRecordData>>
        copy_bootstrap_records(VirAddr uaddr) {
            auto ptrs_res =
                copy_terminated_values<addr_t>(uaddr, static_cast<addr_t>(0));
            propagate(ptrs_res);

            std::vector<TaskSpec::BootstrapRecordData> records{};
            for (addr_t ptr : ptrs_res.value()) {
                UBuffer header_buf(VirAddr(ptr), sizeof(bsheader));
                auto sync_header_res = header_buf.sync_from_user();
                propagate(sync_header_res);
                bsheader header{};
                memcpy(&header, header_buf.kbuf(), sizeof(header));
                if (header.size < sizeof(bsheader) ||
                    header.size > MAX_BOOTSTRAP_RECORD_SIZE)
                {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }

                UBuffer record_buf(VirAddr(ptr), header.size);
                auto sync_record_res = record_buf.sync_from_user();
                propagate(sync_record_res);
                TaskSpec::BootstrapRecordData record{
                    .type  = header.type,
                    .bytes = std::vector<char>(header.size),
                };
                memcpy(record.bytes.data(), record_buf.kbuf(), header.size);
                records.push_back(std::move(record));
            }
            return records;
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
        void write_serial(const UBuffer &str, size_t len) noexcept {
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

        [[nodiscard]]
        b64 time_now_ns() noexcept {
            auto *time_keeper =
                env::hart_ctx != nullptr ? env::hart_ctx->time_keeper() : nullptr;
            if (time_keeper == nullptr || time_keeper->source() == nullptr) {
                return 0;
            }
            return static_cast<b64>(
                time_keeper->source()
                    ->to_ns(time_keeper->source()->now())
                    .to_nanoseconds());
        }

    }  // namespace

    constexpr size_t MAX_SYSCALL_PATH = 256;

    [[nodiscard]]
    Result<StartupArguments> copy_execve_startup(
        const ExecveRequest *request) {
        if (request == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        StartupArguments startup{};
        auto caps_res = copy_terminated_values<CapIdx>(
            VirAddr(reinterpret_cast<addr_t>(request->caps)), cap::null);
        propagate(caps_res);
        startup.caps = std::move(caps_res.value());

        if (request->execfn == nullptr) {
            startup.execfn = "<cap>";
        } else {
            UString execfn(VirAddr(reinterpret_cast<addr_t>(request->execfn)),
                           MAX_SYSCALL_PATH);
            startup.execfn = execfn.kbuf();
        }

        auto argv_res = copy_terminated_strings(
            VirAddr(reinterpret_cast<addr_t>(request->argv)));
        propagate(argv_res);
        startup.argv = std::move(argv_res.value());

        auto envp_res = copy_terminated_strings(
            VirAddr(reinterpret_cast<addr_t>(request->envp)));
        propagate(envp_res);
        startup.envp = std::move(envp_res.value());

        auto bsargv_res = copy_bootstrap_records(
            VirAddr(reinterpret_cast<addr_t>(request->bsargv)));
        propagate(bsargv_res);
        startup.bsargv = std::move(bsargv_res.value());
        return startup;
    }

    const char *name_of(b64 sysno) {
        switch (sysno) {
            case SYS_WRITE_SERIAL:        return "SYS_WRITE_SERIAL";
            case SYS_CREATE_PROCESS:      return "SYS_CREATE_PROCESS";
            case SYS_CREATE_POSIX_PROCESS:return "SYS_CREATE_POSIX_PROCESS";
            case SYS_SHUTDOWN:            return "SYS_SHUTDOWN";
            case SYS_TIME_NOW_NS:         return "SYS_TIME_NOW_NS";
            case SYS_TCB_NANOSLEEP:       return "SYS_TCB_NANOSLEEP";
            case SYS_PCB_KILL:            return "SYS_PCB_KILL";
            case SYS_TCB_KILL:            return "SYS_TCB_KILL";
            case SYS_PCB_FORK:            return "SYS_PCB_FORK";
            case SYS_PCB_GETPID:          return "SYS_PCB_GETPID";
            case SYS_PCB_CREATE_THREAD:   return "SYS_PCB_CREATE_THREAD";
            case SYS_TCB_YIELD:           return "SYS_TCB_YIELD";
            case SYS_PCB_EXECVE:          return "SYS_PCB_EXECVE";
            case SYS_TCB_WAIT:            return "SYS_TCB_WAIT";
            case SYS_TCB_GET_TID:         return "SYS_TCB_GET_TID";
            case SYS_PCB_MAP:             return "SYS_PCB_MAP";
            case SYS_PCB_UNMAP:           return "SYS_PCB_UNMAP";
            case SYS_PCB_QUERY_VADDR:     return "SYS_PCB_QUERY_VADDR";
            case SYS_PCB_QUERY_VSPACE:    return "SYS_PCB_QUERY_VSPACE";
            case SYS_PCB_PROCFS_GET:      return "SYS_PCB_PROCFS_GET";
            case SYS_PCB_PROCFS_REDIRECT: return "SYS_PCB_PROCFS_REDIRECT";
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
            case SYS_VFS_UNLINK:          return "SYS_VFS_UNLINK";
            case SYS_VFS_RMDIR:           return "SYS_VFS_RMDIR";
            case SYS_VFS_TRUNCATE:        return "SYS_VFS_TRUNCATE";
            case SYS_VFS_RENAME:          return "SYS_VFS_RENAME";
            case SYS_VFS_SYMLINK:         return "SYS_VFS_SYMLINK";
            case SYS_VFS_LINK:           return "SYS_VFS_LINK";
            case SYS_VFS_STAT:           return "SYS_VFS_STAT";
            case SYS_VFS_LSTAT:          return "SYS_VFS_LSTAT";
            case SYS_VFS_READLINK:       return "SYS_VFS_READLINK";
            case SYS_VFS_FSTAT:          return "SYS_VFS_FSTAT";
            case SYS_MNT_CREATE:         return "SYS_MNT_CREATE";
            case SYS_MNT_MOUNT:          return "SYS_MNT_MOUNT";
            case SYS_MNT_UMOUNT:         return "SYS_MNT_UMOUNT";
            case SYS_MNT_ROOT:           return "SYS_MNT_ROOT";
            case SYS_MNT_STATE:          return "SYS_MNT_STATE";
            case SYS_PIPE_CREATE:        return "SYS_PIPE_CREATE";
            case SYS_PIPE_READ:          return "SYS_PIPE_READ";
            case SYS_PIPE_WRITE:         return "SYS_PIPE_WRITE";
            case SYS_VFS_PAGE_CACHE_STATS:
                return "SYS_VFS_PAGE_CACHE_STATS";
            case SYS_PCB_EXECVE_POSIX:  return "SYS_PCB_EXECVE_POSIX";
            case SYS_VFS_FCHOWNAT:      return "SYS_VFS_FCHOWNAT";
            default:                      return "UNKNOWN_SYSCALL";
        }
    }

    bool is_linux_syscall_number(b64 sysno) noexcept {
        if ((sysno & 0xFFFF0000ULL) == SYSCALL_BASE) {
            return false;
        }
        if ((sysno & 0xFFC00000ULL) == SYS_UNSTABLE_BASE) {
            return false;
        }
        return true;
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
                UBuffer str((VirAddr)arg0, arg1);
                str.sync_from_user();
                write_serial(str, arg1);
                break;
            }
            case SYS_SHUTDOWN: {
                sys_shutdown();
                __builtin_unreachable();
            }
            case SYS_TIME_NOW_NS: {
                ret.ret0 = time_now_ns();
                break;
            }
            case SYS_TCB_NANOSLEEP: {
                ret = result_void_ret("tcb_nanosleep", tcb_nanosleep(arg0));
                break;
            }
            case SYS_VFS_PAGE_CACHE_STATS: {
                UBuffer buf((VirAddr)arg0, sizeof(VFSPageCacheStats));
                ret = result_void_ret(
                    "vfs_page_cache_stats",
                    vfs_page_cache_stats(std::move(buf), arg1 != 0));
                break;
            }
            case SYS_CREATE_PROCESS: {
                StartupArguments startup{};
                UBuffer req_buf((VirAddr)arg1, sizeof(ExecveRequest));
                auto req_sync_res = req_buf.sync_from_user();
                if (!req_sync_res.has_value()) {
                    ret = result_void_ret("同步创建进程请求", req_sync_res);
                    break;
                }
                auto *request =
                    reinterpret_cast<const ExecveRequest *>(req_buf.kbuf());
                if (request == nullptr) {
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1      = static_cast<b64>(ErrCode::NULLPTR)};
                    break;
                }
                if (request->execfn == nullptr) {
                    startup.execfn = "<cap>";
                } else {
                    UString execfn(
                        VirAddr(reinterpret_cast<addr_t>(request->execfn)),
                        MAX_SYSCALL_PATH);
                    startup.execfn = execfn.kbuf();
                }
                auto caps_res = copy_terminated_values<CapIdx>(
                    VirAddr(reinterpret_cast<addr_t>(request->caps)),
                    cap::null);
                if (!caps_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步进程能力列表失败: err=%s",
                                            to_cstring(caps_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(caps_res.error())};
                    break;
                }
                startup.caps = std::move(caps_res.value());
                auto argv_res = copy_terminated_strings(
                    VirAddr(reinterpret_cast<addr_t>(request->argv)));
                if (!argv_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步进程argv失败: err=%s",
                                            to_cstring(argv_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(argv_res.error())};
                    break;
                }
                startup.argv = std::move(argv_res.value());
                auto envp_res = copy_terminated_strings(
                    VirAddr(reinterpret_cast<addr_t>(request->envp)));
                if (!envp_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步进程envp失败: err=%s",
                                            to_cstring(envp_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(envp_res.error())};
                    break;
                }
                startup.envp = std::move(envp_res.value());
                auto bsargv_res = copy_bootstrap_records(
                    VirAddr(reinterpret_cast<addr_t>(request->bsargv)));
                if (!bsargv_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步进程bsargv失败: err=%s",
                                            to_cstring(bsargv_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 =
                                      static_cast<b64>(bsargv_res.error())};
                    break;
                }
                startup.bsargv = std::move(bsargv_res.value());
                ret = result_value_ret(
                    "创建进程",
                    pcb_create_process(capidx, request->image_cap, arg0,
                                       startup));
                break;
            }
            case SYS_CREATE_POSIX_PROCESS: {
                StartupArguments startup{};
                UBuffer req_buf((VirAddr)arg1, sizeof(ExecveRequest));
                auto req_sync_res = req_buf.sync_from_user();
                if (!req_sync_res.has_value()) {
                    ret = result_void_ret("同步创建POSIX进程请求", req_sync_res);
                    break;
                }
                auto *request =
                    reinterpret_cast<const ExecveRequest *>(req_buf.kbuf());
                if (request == nullptr) {
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1      = static_cast<b64>(ErrCode::NULLPTR)};
                    break;
                }
                if (request->execfn == nullptr) {
                    startup.execfn = "<cap>";
                } else {
                    UString execfn(
                        VirAddr(reinterpret_cast<addr_t>(request->execfn)),
                        MAX_SYSCALL_PATH);
                    startup.execfn = execfn.kbuf();
                }
                auto caps_res = copy_terminated_values<CapIdx>(
                    VirAddr(reinterpret_cast<addr_t>(request->caps)),
                    cap::null);
                if (!caps_res.has_value()) {
                    loggers::SYSCALL::ERROR(
                        "同步POSIX进程能力列表失败: err=%s",
                        to_cstring(caps_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(caps_res.error())};
                    break;
                }
                startup.caps = std::move(caps_res.value());
                auto argv_res = copy_terminated_strings(
                    VirAddr(reinterpret_cast<addr_t>(request->argv)));
                if (!argv_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步POSIX进程argv失败: err=%s",
                                            to_cstring(argv_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(argv_res.error())};
                    break;
                }
                startup.argv = std::move(argv_res.value());
                auto envp_res = copy_terminated_strings(
                    VirAddr(reinterpret_cast<addr_t>(request->envp)));
                if (!envp_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步POSIX进程envp失败: err=%s",
                                            to_cstring(envp_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(envp_res.error())};
                    break;
                }
                startup.envp = std::move(envp_res.value());
                auto bsargv_res = copy_bootstrap_records(
                    VirAddr(reinterpret_cast<addr_t>(request->bsargv)));
                if (!bsargv_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步POSIX进程bsargv失败: err=%s",
                                            to_cstring(bsargv_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 =
                                      static_cast<b64>(bsargv_res.error())};
                    break;
                }
                startup.bsargv = std::move(bsargv_res.value());
                ret = result_value_ret(
                    "创建POSIX进程",
                    pcb_create_linux_process(capidx, request->image_cap, arg0,
                                             startup));
                break;
            }
            case SYS_PCB_CREATE_THREAD: {
                ret = result_value_ret("创建线程",
                                       pcb_create_thread(capidx, VirAddr(arg0),
                                                         VirAddr(arg1), arg2));
                break;
            }
            case SYS_TCB_YIELD: {
                schd::Scheduler::inst().yield();
                ret = RetPack{
                    .processed = true,
                    .ret0      = 0,
                    .ret1      = static_cast<b64>(ErrCode::SUCCESS),
                };
                break;
            }
            case SYS_TCB_WAIT: {
                auto caps_res = copy_terminated_values<CapIdx>(
                    VirAddr(arg0), cap::null);
                if (!caps_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步等待PCB列表失败: err=%s",
                                            to_cstring(caps_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = 0,
                                  .ret1 = static_cast<b64>(caps_res.error())};
                    break;
                }
                UBuffer status_buf((VirAddr)arg1, sizeof(int));
                UBuffer *status_buf_ptr = nullptr;
                if (arg1 != 0) {
                    status_buf_ptr = &status_buf;
                }
                ret = result_value_ret("等待进程状态改变",
                                       tcb_wait(capidx, caps_res.value(),
                                                status_buf_ptr, arg2));
                break;
            }
            case SYS_TCB_GET_TID: {
                ret = result_value_ret("获取线程tid", tcb_get_tid(capidx));
                break;
            }
            case SYS_PCB_FORK: {
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
            case SYS_PCB_EXECVE: {
                StartupArguments startup{};
                UBuffer req_buf((VirAddr)arg0, sizeof(ExecveRequest));
                auto req_sync_res = req_buf.sync_from_user();
                if (!req_sync_res.has_value()) {
                    ret = result_void_ret("同步execve请求", req_sync_res);
                    break;
                }
                auto *request =
                    reinterpret_cast<const ExecveRequest *>(req_buf.kbuf());
                if (request == nullptr) {
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1      = static_cast<b64>(ErrCode::NULLPTR)};
                    break;
                }
                auto caps_res = copy_terminated_values<CapIdx>(
                    VirAddr(reinterpret_cast<addr_t>(request->caps)),
                    cap::null);
                if (!caps_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步execve保留能力列表失败: err=%s",
                                            to_cstring(caps_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(caps_res.error())};
                    break;
                }
                startup.caps = std::move(caps_res.value());
                if (request->execfn == nullptr) {
                    startup.execfn = "<cap>";
                } else {
                    UString execfn(
                        VirAddr(reinterpret_cast<addr_t>(request->execfn)),
                        MAX_SYSCALL_PATH);
                    startup.execfn = execfn.kbuf();
                }
                auto argv_res = copy_terminated_strings(
                    VirAddr(reinterpret_cast<addr_t>(request->argv)));
                if (!argv_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步execve argv失败: err=%s",
                                            to_cstring(argv_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(argv_res.error())};
                    break;
                }
                startup.argv = std::move(argv_res.value());
                auto envp_res = copy_terminated_strings(
                    VirAddr(reinterpret_cast<addr_t>(request->envp)));
                if (!envp_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步execve envp失败: err=%s",
                                            to_cstring(envp_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 = static_cast<b64>(envp_res.error())};
                    break;
                }
                startup.envp = std::move(envp_res.value());
                auto bsargv_res = copy_bootstrap_records(
                    VirAddr(reinterpret_cast<addr_t>(request->bsargv)));
                if (!bsargv_res.has_value()) {
                    loggers::SYSCALL::ERROR("同步execve bsargv失败: err=%s",
                                            to_cstring(bsargv_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 =
                                      static_cast<b64>(bsargv_res.error())};
                    break;
                }
                startup.bsargv = std::move(bsargv_res.value());
                ret = result_bool_ret(
                    "execve", pcb_execve(capidx, request->image_cap, startup));
                break;
            }
            case SYS_PCB_EXECVE_POSIX: {
                UBuffer req_buf((VirAddr)arg0, sizeof(ExecveRequest));
                auto req_sync_res = req_buf.sync_from_user();
                if (!req_sync_res.has_value()) {
                    ret = result_void_ret("同步POSIX execve请求", req_sync_res);
                    break;
                }
                auto *request =
                    reinterpret_cast<const ExecveRequest *>(req_buf.kbuf());
                auto startup_res = copy_execve_startup(request);
                if (!startup_res.has_value()) {
                    loggers::SYSCALL::ERROR(
                        "同步POSIX execve启动参数失败: err=%s",
                        to_cstring(startup_res.error()));
                    ret = RetPack{.processed = true,
                                  .ret0      = false,
                                  .ret1 =
                                      static_cast<b64>(startup_res.error())};
                    break;
                }
                const bool target_current = pcb_is_current(capidx);
                ret = result_bool_ret(
                    "POSIX execve",
                    pcb_execve_linux(capidx, request->image_cap,
                                     startup_res.value()));
                if (ret.ret0 != 0 && target_current) {
                    return ret;
                }
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
            case SYS_VFS_UNLINK: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_void_ret("unlink", vfs_unlink(capidx, path));
                break;
            }
            case SYS_VFS_RMDIR: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_void_ret("rmdir", vfs_rmdir(capidx, path));
                break;
            }
            case SYS_VFS_TRUNCATE: {
                ret = result_void_ret("truncate", vfs_truncate(capidx, arg0));
                break;
            }
            case SYS_VFS_RENAME: {
                UString old_name((VirAddr)arg0, MAX_SYSCALL_PATH);
                UString new_name((VirAddr)arg2, MAX_SYSCALL_PATH);
                ret = result_void_ret("rename",
                    vfs_rename(capidx, old_name, arg1, new_name));
                break;
            }
            case SYS_VFS_SYMLINK: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                UString target((VirAddr)arg1, MAX_SYSCALL_PATH);
                ret = result_void_ret(
                    "symlink", vfs_symlink(capidx, path, target));
                break;
            }
            case SYS_VFS_LINK: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_void_ret(
                    "link", vfs_link(capidx, path, arg1));
                break;
            }
            case SYS_VFS_STAT: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                UBuffer buf((VirAddr)arg1, sizeof(NodeMeta));
                ret = result_void_ret("stat",
                                      vfs_stat(capidx, path, std::move(buf)));
                break;
            }
            case SYS_VFS_LSTAT: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                UBuffer buf((VirAddr)arg1, sizeof(NodeMeta));
                ret = result_void_ret("lstat",
                                      vfs_lstat(capidx, path, std::move(buf)));
                break;
            }
            case SYS_VFS_FSTAT: {
                UBuffer buf((VirAddr)arg0, sizeof(NodeMeta));
                ret = result_void_ret("fstat",
                                      vfs_fstat(capidx, std::move(buf)));
                break;
            }
            case SYS_VFS_READLINK: {
                UString path((VirAddr)arg0, MAX_SYSCALL_PATH);
                UBuffer buf((VirAddr)arg1, arg2);
                ret = result_value_ret(
                    "readlink",
                    vfs_readlink(capidx, path, std::move(buf), arg2));
                break;
            }
            case SYS_VFS_FCHOWNAT: {
                UString relpath((VirAddr)arg3, MAX_SYSCALL_PATH);
                ret = result_void_ret(
                    "fchownat",
                    vfs_fchownat(capidx, relpath,
                                 static_cast<uint32_t>(arg0),
                                 static_cast<uint32_t>(arg1),
                                 static_cast<uint32_t>(arg2)));
                break;
            }
            case SYS_MNT_CREATE: {
                UString fs_name((VirAddr)arg0, MAX_SYSCALL_PATH);
                std::optional<UString> options;
                if (arg2 != 0) {
                    options.emplace(VirAddr(arg2), MAX_SYSCALL_PATH);
                }
                ret = result_value_ret(
                    "mnt_create",
                    mnt_create(capidx, fs_name, arg1,
                               options ? &*options : nullptr));
                break;
            }
            case SYS_MNT_MOUNT: {
                UString mountpoint((VirAddr)arg1, MAX_SYSCALL_PATH);
                ret = result_bool_ret("mnt_mount",
                                      mnt_mount(capidx, arg0, mountpoint, arg2));
                break;
            }
            case SYS_MNT_UMOUNT: {
                ret = result_bool_ret("mnt_umount", mnt_umount(capidx, arg0));
                break;
            }
            case SYS_MNT_ROOT: {
                ret = result_value_ret("mnt_root", mnt_root(capidx));
                break;
            }
            case SYS_MNT_STATE: {
                ret = RetPack{.processed = true,
                              .ret0      = static_cast<b64>(mnt_state(capidx)),
                              .ret1      = static_cast<b64>(ErrCode::SUCCESS)};
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
            case SYS_PIPE_CREATE: {
                UBuffer out_buf((VirAddr)arg0, sizeof(PipeCreateRet));
                ret = result_void_ret("pipe_create",
                                      pipe_create(capidx != 0,
                                                  std::move(out_buf)));
                break;
            }
            case SYS_PIPE_READ: {
                UBuffer buf((VirAddr)arg0, arg1);
                ret = result_value_ret(
                    "pipe_read", pipe_read(capidx, std::move(buf), arg1));
                break;
            }
            case SYS_PIPE_WRITE: {
                UBuffer buf((VirAddr)arg0, arg1);
                auto sync_res = buf.sync_from_user();
                if (!sync_res.has_value()) {
                    ret = result_void_ret("同步pipe write缓冲区", sync_res);
                    break;
                }
                ret = result_value_ret(
                    "pipe_write", pipe_write(capidx, std::move(buf), arg1));
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
            case SYS_TCB_KILL: {
                ret = result_bool_ret("tcb_kill",
                                      tcb_kill(capidx, static_cast<int>(arg0)));
                break;
            }
            case SYS_PCB_GETPID: {
                ret = result_value_ret("get_pid", get_pid(capidx));
                break;
            }
            case SYS_PCB_PROCFS_GET: {
                UString name((VirAddr)arg0, MAX_SYSCALL_PATH);
                ret = result_value_ret("procfs_get",
                                       pcb_procfs_get(capidx, name));
                break;
            }
            case SYS_PCB_PROCFS_REDIRECT: {
                UString name((VirAddr)arg0, MAX_SYSCALL_PATH);
                UString target((VirAddr)arg1, MAX_SYSCALL_PATH);
                ret = result_bool_ret("procfs_redirect",
                                      pcb_procfs_redirect(capidx, name, target));
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
                    mem_create(capidx, arg0, arg1, arg2,
                               static_cast<cap::MemoryGrowth>(arg3), arg4));
                break;
            }
            case SYS_PCB_MAP: {
                ret = result_bool_ret(
                    "pcb_map",
                    pcb_map(capidx, static_cast<CapIdx>(arg0), arg1,
                            VirAddr(arg2), arg3, arg4));
                break;
            }
            case SYS_PCB_UNMAP: {
                ret = result_bool_ret("pcb_unmap",
                                      pcb_unmap(capidx, VirAddr(arg0), arg1));
                break;
            }
            case SYS_PCB_QUERY_VADDR: {
                UBuffer info_buf((VirAddr)arg1, sizeof(cap::VMAInfo));
                auto query_res = pcb_query_vaddr(
                    capidx, VirAddr(arg0), std::move(info_buf),
                    pcb_is_current(capidx));
                ret = result_void_ret("pcb_query_vaddr", query_res);
                break;
            }
            case SYS_PCB_QUERY_VSPACE: {
                UBuffer info_buf((VirAddr)arg1,
                                 arg2 * sizeof(cap::VMAInfo));
                auto query_res =
                    pcb_query_vspace(capidx, arg0, std::move(info_buf), arg2,
                                     pcb_is_current(capidx));
                ret = result_value_ret("pcb_query_vspace", query_res);
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
