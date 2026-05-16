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

#include <env.h>
#include <logger.h>
#include <sustcore/addr.h>
#include <sustcore/syscall.h>
#include <syscall/cap.h>
#include <syscall/notif.h>
#include <syscall/syscall.h>
#include <syscall/task.h>
#include <syscall/uaccess.h>

namespace syscall {
    void write_serial(const UString &str, size_t len) {
        kwrites(str.kbuf(), len);
    }

    constexpr size_t MAX_SYSCALL_PATH = 256;

    VirArea sys_grow_vma(VirAddr vaddr, VirArea new_area) {
        // 定位当前 VMA 并尝试增长到新的范围
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

    RetPack entrance(const ArgPack &args) {
        // 参数解包
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
            case SYS_FORK: {
                auto fork_ret = fork();
                ret0          = fork_ret.child_pcb_cap;
                ret1          = fork_ret.child_pid;
                break;
            }
            case SYS_EXIT: {
                exit();
                ret0 = ret1 = 0;
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
            case SYS_CAP_REMOVE: {
                ret0 = cap_remove(capidx);
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
