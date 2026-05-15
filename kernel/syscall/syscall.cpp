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

#include <cap/cholder.h>
#include <env.h>
#include <logger.h>
#include <mem/kaddr.h>
#include <object/notif.h>
#include <object/task.h>
#include <perm/permission.h>
#include <schd/schdbase.h>
#include <sus/owner.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>
#include <sustcore/syscall.h>
#include <syscall/syscall.h>

namespace syscall {
    // 将用户空间中的数据读取到内核空间中
    class UBuffer {
    private:
        VirAddr _uaddr;             // 用户空间地址
        util::owner<char *> _kbuf;  // 内核空间缓冲区地址
        size_t _len;                // 数据长度
    public:
        UBuffer(VirAddr uaddr, size_t len) : _uaddr(uaddr), _len(len) {
            // 分配内核空间缓冲区
            _kbuf = util::owner(new char[_len]);
        }

        ~UBuffer() {
            loggers::SYSCALL::DEBUG("释放内核缓冲区: %p", _kbuf.get());
            delete[] _kbuf;
        }

        UBuffer &sync_from_user() {
            // 从用户空间复制数据到内核空间
            // 打开guard
            ker_paddr::SumGuard guard;
            guard.open();
            memcpy(_kbuf, _uaddr.addr(), _len);
            return *this;
        }

        UBuffer &sync_to_user() {
            // 从内核空间复制数据到用户空间
            ker_paddr::SumGuard guard;
            guard.open();
            memcpy(_uaddr.addr(), _kbuf, _len);
            return *this;
        }

        [[nodiscard]] char *kbuf() const {
            return _kbuf;
        }

        [[nodiscard]] size_t len() const {
            return _len;
        }

        [[nodiscard]] VirAddr uaddr() const {
            return _uaddr;
        }
    };

    // 用户空间字符串(readonly)
    class UString {
    private:
        UBuffer _ubuf;
        int _len;

    public:
        UString(VirAddr uaddr, size_t maxlen) : _ubuf(uaddr, maxlen) {
            sync_from_user();
        }

        ~UString() {}

        UString &sync_from_user() {
            _ubuf.sync_from_user();
            _len = strnlen(kbuf(), maxlen());
            return *this;
        }

        [[nodiscard]] char *kbuf() const {
            return _ubuf.kbuf();
        }

        [[nodiscard]] size_t len() const {
            return _len;
        }

        [[nodiscard]] size_t maxlen() const {
            return _ubuf.len();
        }

        [[nodiscard]] VirAddr uaddr() const {
            return _ubuf.uaddr();
        }
    };

    void write_serial(const UString &str, size_t len) {
        kwrites(str.kbuf(), len);
    }

    constexpr size_t MAX_SYSCALL_PATH = 256;

    CapIdx create_process(const UString &path, VirAddr caps_uaddr,
                          size_t caps_sz) {
        // Create children process
        auto current_holder_res = cap::CHolder::current();
        if (!current_holder_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 当前CSpace不可用");
            return cap::error;
        }
        cap::CHolder *current_holder = current_holder_res.value();

        // load children ELF
        auto load_res =
            env::inst().tm()->load_elf(path.kbuf(), schd::ClassType::RR);
        if (!load_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: path=%s, 错误码: %s",
                                    path.kbuf(), to_cstring(load_res.error()));
            return cap::error;
        }
        // create a guard to automatically clean up the loaded PCB object if any error occurs later,
        auto pcb_guard = util::Guard([&]() {
            if (load_res.has_value()) {
                auto pcb = load_res.value();
                loggers::SYSCALL::INFO("清理已加载的PCB对象: pid=%d", pcb->pid);
                // TODO: 调用 task manager 的接口来删除 PCB 对象
                // env::inst().tm().remove_pcb(pcb->pid);
            }
        });

        auto pcb = load_res.value();
        auto child_pcb_cap_res = pcb->cholder->internal_lookup(pcb->pcb_cap);
        if (!child_pcb_cap_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 子进程PCB能力不可用 err=%d",
                                    child_pcb_cap_res.error());
            return cap::error;
        }

        cap::PCBObject child_pcb_obj(
            util::nnullforce(child_pcb_cap_res.value()));

        if (caps_sz != 0) {
            UBuffer caps_buf(caps_uaddr, caps_sz * sizeof(CapIdx));
            caps_buf.sync_from_user();
            auto *caps = reinterpret_cast<CapIdx *>(caps_buf.kbuf());
            for (size_t i = 0; i < caps_sz; ++i) {
                auto insert_res = child_pcb_obj.cap_insert(caps[i], *current_holder);
                if (!insert_res.has_value()) {
                    loggers::SYSCALL::ERROR(
                        "创建进程失败: 初始能力插入失败 idx=%d err=%d", i,
                        insert_res.error());
                    return cap::error;
                }
            }
        }

        auto ret_slot_res = current_holder->internal_lookup_freeslot();
        if (!ret_slot_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 调用者无空闲能力槽 err=%d",
                                    ret_slot_res.error());
            return cap::error;
        }
        auto ret_insert_res = current_holder->internal_insert(
            ret_slot_res.value(), child_pcb_cap_res.value()->payload(),
            child_pcb_cap_res.value()->perm());
        if (!ret_insert_res.has_value()) {
            loggers::SYSCALL::ERROR("创建进程失败: 返回PCB能力插入失败 err=%d",
                                    ret_insert_res.error());
            return cap::error;
        }

        loggers::SYSCALL::INFO("创建进程成功: path=%s, pid=%d", path.kbuf(),
                               pcb->pid);
        return ret_slot_res.value();
    }

    VirArea sys_grow_vma(VirAddr vaddr, VirArea new_area) {
        auto tmm        = env::inst().tmm();
        auto locate_res = tmm->locate(vaddr);
        if (!locate_res.has_value()) {
            loggers::SYSCALL::ERROR("无法找到包含地址 %p 的 VMA: err=%d",
                                    vaddr.addr(), locate_res.error());
            return {};
        }
        auto vma      = locate_res.value();
        auto grow_res = tmm->grow_vma(vma, new_area);
        if (!grow_res.has_value()) {
            loggers::SYSCALL::ERROR("无法增长 VMA: err=%d", grow_res.error());
            return vma->varea;
        }
        return grow_res.value();
    }

    Result<cap::NotificationObject> notif_object(CapIdx capidx) {
        auto cap_res = cap::CHolder::lookup(capidx);
        propagate(cap_res);
        auto *cap = cap_res.value();
        if (cap->payload()->type_id() != PayloadType::NOTIF) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        return cap::NotificationObject(util::nnullforce(cap));
    }

    // TODO: allow syscall to directly return an result,
    // and when returning an error, let the return value to be -errcode,
    // and automatically log the error in the syscall_entrance function,
    // to avoid repetitive error handling code in each syscall handler.
    bool wait_notification(CapIdx capidx, size_t idx) {
        auto notif_res = notif_object(capidx).and_then(
            [idx](cap::NotificationObject obj) { return obj.wait(idx); });
        if (!notif_res.has_value()) {
            loggers::SYSCALL::ERROR("等待notification失败: err=%d",
                                    notif_res.error());
            return false;
        }
        return notif_res.value();
    }

    bool signal_notification(CapIdx capidx, size_t idx, bool state) {
        auto set_res = notif_object(capidx).and_then(
            [idx, state](cap::NotificationObject obj) { return obj.set(idx, state); });
        if (!set_res.has_value()) {
            loggers::SYSCALL::ERROR("设置notification失败: err=%d",
                                    set_res.error());
            return false;
        }
        return set_res.value();
    }

    bool check_notification(CapIdx capidx, size_t idx) {
        auto query_res = notif_object(capidx).and_then(
            [idx](cap::NotificationObject obj) { return obj.query(idx); });
        if (!query_res.has_value()) {
            loggers::SYSCALL::ERROR("查询notification失败: err=%d",
                                    query_res.error());
            return false;
        }
        return query_res.value();
    }

    bool create_notification(CapIdx capidx) {
        auto create_res = cap::CHolder::create<cap::NotificationPayload>(capidx);
        if (!create_res.has_value()) {
            loggers::SYSCALL::ERROR("创建notification失败: err=%d",
                                    create_res.error());
            return false;
        }
        return true;
    }

    bool cap_clone(CapIdx src, CapIdx target) {
        auto clone_res = cap::CHolder::clone(target, src);
        if (!clone_res.has_value()) {
            loggers::SYSCALL::ERROR("clone capability失败: err=%d",
                                    clone_res.error());
            return false;
        }
        return true;
    }

    bool cap_downgrade(CapIdx idx, b64 new_perm) {
        auto downgrade_res = cap::CHolder::downgrade(idx, new_perm);
        if (!downgrade_res.has_value()) {
            loggers::SYSCALL::ERROR("downgrade capability失败: err=%d",
                                    downgrade_res.error());
            return false;
        }
        return true;
    }

    bool cap_derive(CapIdx src, CapIdx target, b64 new_perm) {
        auto derive_res = cap::CHolder::derive(target, src, new_perm);
        if (!derive_res.has_value()) {
            loggers::SYSCALL::ERROR("derive capability失败: err=%d",
                                    derive_res.error());
            return false;
        }
        return true;
    }

    size_t get_pid(CapIdx pcb_cap) {
        auto cap_res = cap::CHolder::lookup(pcb_cap);
        if (!cap_res.has_value()) {
            loggers::SYSCALL::ERROR("get_pid失败: err=%d", cap_res.error());
            return 0;
        }
        if (cap_res.value()->payload()->type_id() != PayloadType::PCB) {
            loggers::SYSCALL::ERROR("get_pid失败: 类型不匹配");
            return 0;
        }
        auto pid_res =
            cap::PCBObject(util::nnullforce(cap_res.value())).get_pid();
        if (!pid_res.has_value()) {
            loggers::SYSCALL::ERROR("get_pid失败: err=%d", pid_res.error());
            return 0;
        }
        return pid_res.value();
    }

    bool lookup_cap(CapIdx idx, VirAddr info_uaddr) {
        if (!info_uaddr.nonnull()) {
            return false;
        }
        auto cap_res = cap::CHolder::lookup(idx);
        if (!cap_res.has_value()) {
            loggers::SYSCALL::ERROR("lookup_cap失败: err=%d", cap_res.error());
            return false;
        }
        CapInfo info{
            .type        = cap_res.value()->payload()->type_id(),
            .permissions = cap_res.value()->perm(),
        };
        UBuffer info_buf(info_uaddr, sizeof(info));
        memcpy(info_buf.kbuf(), &info, sizeof(info));
        info_buf.sync_to_user();
        return true;
    }

    RetPack entrance(const ArgPack &args) {
        b64 arg0 = args.args[0];
        b64 arg1 = args.args[1];
        b64 arg2 = args.args[2];

        b64 sysno  = args.syscall_number;
        b64 capidx = args.capidx;

        b64 ret0 = 0, ret1 = 0;

        bool processed = true;

        // capidx (a6) is the primary capability slot for capability syscalls;
        // args[] carry operation-specific values.
        switch (sysno) {
            // Basic process / memory syscalls.
            case SYS_WRITE_SERIAL: {
                write_serial(UString((VirAddr)arg0, arg1), arg1);
                ret0 = ret1 = 0;
                break;
            }
            case SYS_CREATE_PROCESS: {
                ret0 = create_process(UString((VirAddr)arg0, MAX_SYSCALL_PATH),
                                      VirAddr(arg1), arg2);
                ret1 = 0;
                break;
            }
            case SYS_GROW_VMA: {
                auto vaddr    = (VirAddr)arg0;
                auto new_area = VirArea((VirAddr)arg1, (VirAddr)arg2);
                auto ret_area = sys_grow_vma(vaddr, new_area);
                ret0          = ret_area.begin.arith();
                ret1          = ret_area.end.arith();
                break;
            }
            case SYS_GETPID: {
                ret0 = get_pid(capidx);
                ret1 = 0;
                break;
            }

            // Notification object operations.
            case SYS_WAIT_NOTIFICATION: {
                ret0 = wait_notification(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_SIGNAL_NOTIFICATION: {
                ret0 = signal_notification(capidx, arg0, true);
                ret1 = 0;
                break;
            }
            case SYS_UNSIGNAL_NOTIFICATION: {
                ret0 = signal_notification(capidx, arg0, false);
                ret1 = 0;
                break;
            }
            case SYS_CHECK_NOTIFICATION: {
                ret0 = check_notification(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_CREATE_NOTIFICATION: {
                ret0 = create_notification(capidx);
                ret1 = 0;
                break;
            }

            // Generic capability operations.
            case SYS_CAP_CLONE: {
                ret0 = cap_clone(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_CAP_DOWNGRADE: {
                ret0 = cap_downgrade(capidx, arg0);
                ret1 = 0;
                break;
            }
            case SYS_CAP_DERIVE: {
                ret0 = cap_derive(capidx, arg0, arg1);
                ret1 = 0;
                break;
            }
            case SYS_LOOKUP_CAP: {
                ret0 = lookup_cap(capidx, VirAddr(arg0));
                ret1 = 0;
                break;
            }
            default: {
                processed = false;
                loggers::SYSCALL::ERROR("未知的系统调用号: %d", sysno);
                ret0 = ret1 = 0;
                break;
            }
        }

        return RetPack{processed, ret0, ret1};
    };
}  // namespace syscall
