/**
 * @file clone.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief clone 系统调用支持
 * @version alpha-1.0.0
 * @date 2026-06-23
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <errno.h>
#include <logger.h>
#include <prog.h>
#include <std/stdio.h>
#include <sus/types.h>
#include <syscall.h>
#include <sys/wait.h>
#include <thread.h>

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <syscall.h.in>

namespace {
    struct CloneFlagName {
        size_t value;
        const char *name;
    };

    constexpr size_t CSIGNAL              = 0x000000ff;
    constexpr size_t CLONE_VM             = 0x00000100;
    constexpr size_t CLONE_FS             = 0x00000200;
    constexpr size_t CLONE_FILES          = 0x00000400;
    constexpr size_t CLONE_SIGHAND        = 0x00000800;
    constexpr size_t CLONE_PIDFD          = 0x00001000;
    constexpr size_t CLONE_PTRACE         = 0x00002000;
    constexpr size_t CLONE_VFORK          = 0x00004000;
    constexpr size_t CLONE_PARENT         = 0x00008000;
    constexpr size_t CLONE_THREAD         = 0x00010000;
    constexpr size_t CLONE_NEWNS          = 0x00020000;
    constexpr size_t CLONE_SYSVSEM        = 0x00040000;
    constexpr size_t CLONE_SETTLS         = 0x00080000;
    constexpr size_t CLONE_PARENT_SETTID  = 0x00100000;
    constexpr size_t CLONE_CHILD_CLEARTID = 0x00200000;
    constexpr size_t CLONE_DETACHED       = 0x00400000;
    constexpr size_t CLONE_UNTRACED       = 0x00800000;
    constexpr size_t CLONE_CHILD_SETTID   = 0x01000000;
    constexpr size_t CLONE_NEWCGROUP      = 0x02000000;
    constexpr size_t CLONE_NEWUTS         = 0x04000000;
    constexpr size_t CLONE_NEWIPC         = 0x08000000;
    constexpr size_t CLONE_NEWUSER        = 0x10000000;
    constexpr size_t CLONE_NEWPID         = 0x20000000;
    constexpr size_t CLONE_NEWNET         = 0x40000000;
    constexpr size_t CLONE_IO             = 0x80000000;
    constexpr size_t CLONE_NEWTIME        = 0x00000080;
    constexpr int WAIT4_WNOHANG           = 1;
    constexpr int WAIT4_ANY_CHILD         = -1;

    constexpr CloneFlagName CLONE_FLAG_NAMES[] = {
        {.value = CLONE_NEWTIME, .name = "CLONE_NEWTIME"},
        {.value = CLONE_VM, .name = "CLONE_VM"},
        {.value = CLONE_FS, .name = "CLONE_FS"},
        {.value = CLONE_FILES, .name = "CLONE_FILES"},
        {.value = CLONE_SIGHAND, .name = "CLONE_SIGHAND"},
        {.value = CLONE_PIDFD, .name = "CLONE_PIDFD"},
        {.value = CLONE_PTRACE, .name = "CLONE_PTRACE"},
        {.value = CLONE_VFORK, .name = "CLONE_VFORK"},
        {.value = CLONE_PARENT, .name = "CLONE_PARENT"},
        {.value = CLONE_THREAD, .name = "CLONE_THREAD"},
        {.value = CLONE_NEWNS, .name = "CLONE_NEWNS"},
        {.value = CLONE_SYSVSEM, .name = "CLONE_SYSVSEM"},
        {.value = CLONE_SETTLS, .name = "CLONE_SETTLS"},
        {.value = CLONE_PARENT_SETTID, .name = "CLONE_PARENT_SETTID"},
        {.value = CLONE_CHILD_CLEARTID, .name = "CLONE_CHILD_CLEARTID"},
        {.value = CLONE_DETACHED, .name = "CLONE_DETACHED"},
        {.value = CLONE_UNTRACED, .name = "CLONE_UNTRACED"},
        {.value = CLONE_CHILD_SETTID, .name = "CLONE_CHILD_SETTID"},
        {.value = CLONE_NEWCGROUP, .name = "CLONE_NEWCGROUP"},
        {.value = CLONE_NEWUTS, .name = "CLONE_NEWUTS"},
        {.value = CLONE_NEWIPC, .name = "CLONE_NEWIPC"},
        {.value = CLONE_NEWUSER, .name = "CLONE_NEWUSER"},
        {.value = CLONE_NEWPID, .name = "CLONE_NEWPID"},
        {.value = CLONE_NEWNET, .name = "CLONE_NEWNET"},
        {.value = CLONE_IO, .name = "CLONE_IO"},
    };

    [[nodiscard]]
    std::string append_hex(size_t value) {
        char buf[32]{};
        snprintf(buf, sizeof(buf), "0x%016lx",
                 static_cast<unsigned long>(value));
        return std::string(buf);
    }

    [[nodiscard]]
    std::string clone_flags_to_string(size_t flags) {
        std::string out{};
        size_t remaining = flags;

        for (const auto &flag_name : CLONE_FLAG_NAMES) {
            if ((flags & flag_name.value) == 0) {
                continue;
            }
            if (!out.empty()) {
                out += " | ";
            }
            out       += flag_name.name;
            remaining &= ~flag_name.value;
        }

        if (remaining != 0) {
            if (!out.empty()) {
                out += " | ";
            }
            out += append_hex(remaining);
        }

        if (out.empty()) {
            out = "0";
        }
        return out;
    }

    [[nodiscard]]
    bool thread_like_clone(size_t flags) noexcept {
        constexpr size_t CLONE_VM_FLAG     = 0x00000100;
        constexpr size_t CLONE_THREAD_FLAG = 0x00010000;
        return (flags & CLONE_VM_FLAG) != 0 && (flags & CLONE_THREAD_FLAG) != 0;
    }

    [[nodiscard]]
    size_t find_child_index_by_cap(CapIdx pcb_cap) noexcept {
        for (size_t i = 0; i < __prog_children.size(); ++i) {
            if (__prog_children[i] == pcb_cap) {
                return i;
            }
        }
        return __prog_children.size();
    }

    [[nodiscard]]
    bool find_child_cap_by_pid(int pid, CapIdx &pcb_cap) noexcept {
        for (auto child_cap : __prog_children) {
            if (sys_getpid(child_cap) == static_cast<size_t>(pid)) {
                pcb_cap = child_cap;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]]
    std::vector<CapIdx> make_wait_caps_vector() {
        std::vector<CapIdx> wait_caps{};
        wait_caps.reserve(__prog_children.size() + 1);
        for (auto child_cap : __prog_children) {
            wait_caps.push_back(child_cap);
        }
        wait_caps.push_back(cap::null);
        return wait_caps;
    }
}  // namespace

size_t clone_process(size_t flags, addr_t newsp, int *parent_tid,
                     int *child_tid, addr_t tls,
                     addr_t dispatch_frame_sp) {
    loggers::LXSC::DEBUG(
        "clone_process flags=0x%016lx newsp=%p parent_tid=%p child_tid=%p "
        "tls=%p dispatch_frame=%p",
        static_cast<unsigned long>(flags), reinterpret_cast<void *>(newsp),
        parent_tid, child_tid, reinterpret_cast<void *>(tls),
        reinterpret_cast<void *>(dispatch_frame_sp));

    if (newsp != 0) {
        loggers::LXSC::INFO("clone_process 忽略 newsp=%p",
                            reinterpret_cast<void *>(newsp));
    }
    if (parent_tid != nullptr || child_tid != nullptr || tls != 0) {
        loggers::LXSC::INFO("clone_process 忽略 父子 tid 与 tls");
    }

    CapIdx child_pcb_cap = cap::null;
    size_t pid           = sys_pcb_fork(__prog_pcb_cap, &child_pcb_cap);
    if (pid == 0 && child_pcb_cap != cap::null && child_pcb_cap != cap::error) {
        loggers::LXRT::INFO("fork 子进程返回");
        CapIdx parent_pcb_cap = __prog_pcb_cap;
        __prog_parent_cap     = parent_pcb_cap;
        __prog_pcb_cap = child_pcb_cap;
        __prog_children.clear();
        if (newsp != 0) {
            // TODO: 当前仅做 stack pivot，不复制旧调用链依赖的栈内容。
            linux_clone_fork_return_trampoline(newsp, dispatch_frame_sp);
        }
    } else {
        loggers::LXRT::INFO("fork 父进程返回");
        if (pid > 0 && child_pcb_cap != cap::null && child_pcb_cap != cap::error) {
            __prog_children.push_back(child_pcb_cap);
            loggers::LXRT::DEBUG("记录子进程 pid=%lu pcb_cap=%p children=%lu",
                                 static_cast<unsigned long>(pid),
                                 reinterpret_cast<void *>(child_pcb_cap),
                                 static_cast<unsigned long>(__prog_children.size()));
        } else if (pid > 0) {
            loggers::LXRT::ERROR("fork 返回 pid=%lu 但 child_pcb_cap 无效",
                                 static_cast<unsigned long>(pid));
        }
    }
    return pid;
}

CapIdx clone_thread(size_t flags, addr_t newsp, int *parent_tid, int *child_tid,
                    addr_t tls) {
    loggers::LXSC::INFO(
        "clone_thread placeholder flags=0x%016lx newsp=%p parent_tid=%p "
        "child_tid=%p tls=%p",
        static_cast<unsigned long>(flags), reinterpret_cast<void *>(newsp),
        parent_tid, child_tid, reinterpret_cast<void *>(tls));
    return cap::error;
}

size_t linux_sys_clone(size_t flags, addr_t newsp, int *parent_tid,
                       int *child_tid, addr_t tls,
                       addr_t dispatch_frame_sp) {
    auto flags_str = clone_flags_to_string(flags);
    size_t csignal = flags & CSIGNAL;
    loggers::LXSC::DEBUG(
        "clone flags=0x%016lx (%s) newsp=%p parent_tid=%p child_tid=%p tls=%p "
        "dispatch_frame=%p CSIGNAL=0x%02lx",
        static_cast<unsigned long>(flags), flags_str.c_str(),
        reinterpret_cast<void *>(newsp), parent_tid, child_tid,
        reinterpret_cast<void *>(tls),
        reinterpret_cast<void *>(dispatch_frame_sp),
        static_cast<unsigned long>(csignal));

    if (thread_like_clone(flags)) {
        loggers::LXSC::DEBUG("clone dispatch => clone_thread");
        loggers::LXSC::ERROR("thread-like clone is not implemented");
        return -ENOSYS;
    } else {
        loggers::LXSC::DEBUG("clone dispatch => clone_process");
        if (newsp != 0) {
            loggers::LXSC::DEBUG(
                "clone->fork forwarding via helper newsp=%p dispatch_frame=%p",
                reinterpret_cast<void *>(newsp),
                reinterpret_cast<void *>(dispatch_frame_sp));
        }
        return clone_process(flags, newsp, parent_tid, child_tid, tls,
                             dispatch_frame_sp);
    }
    return static_cast<size_t>(-ENOSYS);
}

size_t linux_sys_wait4(int pid, int *status, int options, void *rusage) {
    loggers::LXSC::DEBUG(
        "wait4 pid=%d status=%p options=0x%x rusage=%p",
        pid, status, options, rusage);

    if (rusage != nullptr) {
        loggers::LXSC::ERROR("wait4 暂不支持 rusage");
        return static_cast<size_t>(-ENOSYS);
    }

    if (__prog_children.empty()) {
        loggers::LXRT::ERROR("wait4 没有可等待的子进程");
        return static_cast<size_t>(-ECHILD);
    }

    if (options != 0 && options != WAIT4_WNOHANG) {
        loggers::LXSC::ERROR("wait4 暂不支持 options=0x%x", options);
        return static_cast<size_t>(-EINVAL);
    }

    std::vector<CapIdx> wait_caps{};
    if (pid == WAIT4_ANY_CHILD) {
        wait_caps = make_wait_caps_vector();
    } else {
        CapIdx child_pcb_cap = cap::null;
        if (!find_child_cap_by_pid(pid, child_pcb_cap)) {
            loggers::LXRT::ERROR("wait4 pid=%d 不属于直接子进程", pid);
            return static_cast<size_t>(-ECHILD);
        }
        wait_caps = {child_pcb_cap, cap::null};
    }

    int *status_ptr     = status;
    size_t wait_options = options == WAIT4_WNOHANG
                              ? static_cast<size_t>(WAIT4_WNOHANG)
                              : 0;
    CapIdx waited_cap =
        sys_tcb_wait(__prog_main_tcb_cap, wait_caps.data(), status_ptr,
                     wait_options);
    if (waited_cap == cap::null) {
        loggers::LXSC::INFO("wait4 WNOHANG 未等到子进程退出");
        return 0;
    }
    if (waited_cap == cap::error) {
        loggers::LXSC::ERROR("wait4 syscall 失败");
        return static_cast<size_t>(-ECHILD);
    }

    const size_t child_pid = sys_getpid(waited_cap);
    size_t child_index     = find_child_index_by_cap(waited_cap);
    if (child_index == __prog_children.size()) {
        loggers::LXRT::ERROR("wait4 返回未知子进程能力=%p",
                             reinterpret_cast<void *>(waited_cap));
        return static_cast<size_t>(-ECHILD);
    }
    __prog_children.erase(__prog_children.begin() +
                          static_cast<std::ptrdiff_t>(child_index));
    if (!sys_cap_remove(waited_cap)) {
        loggers::LXRT::ERROR("wait4 无法移除已退出子进程能力=%p",
                             reinterpret_cast<void *>(waited_cap));
    }
    loggers::LXRT::INFO("wait4 等到子进程 pid=%lu 退出",
                        static_cast<unsigned long>(child_pid));
    return child_pid;
}
