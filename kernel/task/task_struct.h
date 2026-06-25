/**
 * @file task_struct.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <fwd.h>
#include <arch/description.h>
#include <cap/cholder.h>
#include <mem/vma.h>
#include <schd/fcfs.h>
#include <schd/rr.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sus/owner.h>
#include <sus/tree.h>
#include <sustcore/addr.h>

#include <cstddef>
#include <functional>

using tid_t = size_t;
using pid_t = size_t;

namespace wait {
    /// `wd` 是 `wait descriptor` 的缩写, 表示等待子系统分配的描述符编号.
    using wd_t               = size_t;
    using WaitPredicate      = std::function<bool(task::TCB *tcb)>;
    using WaitReadyPredicate = std::function<bool()>;
}  // namespace wait

namespace task {
    enum class BootThreadRole {
        NONE,
        KINIT,
        INIT_USER,
    };

    constexpr static VirAddr GENERIC_PROCESS_BASE =
        VirAddr(static_cast<addr_t>(0x0000000001000000ULL));
    constexpr static VirAddr GENERIC_INTERPRET_BASE =
        VirAddr(static_cast<addr_t>(0x0000000000800000ULL));
    constexpr static VirAddr GENERIC_SUBSYSTEM_BASE =
        VirAddr(static_cast<addr_t>(0x0000000040000000ULL));

    using KThreadEntry = void (*)(void *);

    // Make sure that TCB is has standard layout,
    // so that we can use offsetof to get the TCB pointer from the SU pointer.
    // TCB are arranged as a linked list
    struct TCB {
        /**
         * @brief 单个线程的 syscall 生命周期信息.
         */
        struct SyscallInfo {
            /**
             * @brief syscall 状态.
             */
            enum class State {
                NONE,
                EXECUTING,
                COMPLETED,
            };

            /// dispatcher 记录的 syscall 参数.
            syscall::ArgPack syscall_args{};
            /// 当前正在执行的 syscall 号.
            b64 syscall_number = 0;
            /// syscall 返回值缓存.
            syscall::RetPack syscall_result{};
            /// syscall 当前状态.
            State syscall_state = State::NONE;

            /**
             * @brief 清空 syscall 生命周期状态.
             */
            void reset() noexcept;

            /**
             * @brief 以新的参数启动 syscall 生命周期.
             *
             * @param args 本次 syscall 的寄存器参数包.
             */
            void begin(const syscall::ArgPack &args) noexcept;

            /**
             * @brief 将 syscall 标记为完成并缓存返回值.
             *
             * @param ret syscall 返回结果包.
             */
            void complete(const syscall::RetPack &ret) noexcept;

            /**
             * @brief 判断当前 syscall 是否仍在执行中.
             */
            [[nodiscard]]
            bool executing() const noexcept {
                return syscall_state == State::EXECUTING;
            }

            /**
             * @brief 判断当前 syscall 是否已经完成.
             */
            [[nodiscard]]
            bool completed() const noexcept {
                return syscall_state == State::COMPLETED;
            }
        };

        // thread info
        tid_t tid;
        PCB *task;
        bool is_kernel;
        KThreadEntry kentry;
        void *karg;
        util::ListHead<TCB> list_head;

        // running information
        constexpr static size_t KSTACK_PAGES = 64;  // 256KB(64 pages) for kernel stack
        constexpr static size_t KSTACK_SIZE = KSTACK_PAGES * PAGESIZE;
        void *kstack_bottom;
        char *ksp;
        PhyAddr kstack_phy;
        Context kernel_ctx;

        [[nodiscard]]
        void *kstack_top() const noexcept {
            return kstack_bottom;
        }

        void reset_kstack() noexcept {
            ksp = (char *)kstack_bottom;
        }

        template <typename T>
        [[nodiscard]]
        T *push() noexcept {
            auto next = ksp - sizeof(T);
            ksp       = next;
            return reinterpret_cast<T *>(next);
        }

        [[nodiscard]]
        Context *kernel_context_ptr() noexcept {
            return &kernel_ctx;
        }

        [[nodiscard]]
        const Context *kernel_context_ptr() const noexcept {
            return &kernel_ctx;
        }

        [[nodiscard]]
        Context *context() noexcept {
            if (is_kernel) {
                return kernel_context_ptr();
            }
            auto *top = reinterpret_cast<char *>(kstack_bottom);
            return reinterpret_cast<Context *>(top - sizeof(Context));
        }

        [[nodiscard]]
        const Context *context() const noexcept {
            if (is_kernel) {
                return kernel_context_ptr();
            }
            auto *top = reinterpret_cast<const char *>(kstack_bottom);
            return reinterpret_cast<const Context *>(top - sizeof(Context));
        }

        //  schedule data
        BootThreadRole boot_role;
        schd::ClassType schd_class;
        schd::SchedMeta basic_entity;
        schd::rr::Entity rr_entity;

        // wait data
        util::ListHead<TCB> wait_head;
        wait::wd_t wait_wd;
        // 等待谓词, 由等待的线程在进入等待时设置,
        // 由被等待的事件在满足条件时检查, 决定是否可以唤醒线程
        wait::WaitPredicate wait_predicate;
        SyscallInfo syscall_info;

        void *operator new(size_t size);
        void operator delete(void *ptr);
    };

    // PCB are arranged as a tree
    struct PCB : public util::tree_base::TreeBase<PCB> {
        // process info
        pid_t pid;
        bool is_kernel;
        int exit_code;
        std::atomic<bool> exiting;
        // 是否已被加入回收队列
        std::atomic<bool> recycle_queued;

        // the threads in this process
        util::IntrusiveList<TCB, &TCB::list_head> threads;

        // resources
        util::owner<TaskMemoryManager *> tmm;
        cap::CHolder *cholder;

        // initialization information
        VirAddr entrypoint;
        VirAddr linuxproc_entrypoint;
        VirAddr linux_subsystem_entry;
        bool is_linux_process;
        CapIdx pcb_cap;
        CapIdx main_tcb_cap;

        void *operator new(size_t size);
        void operator delete(void *ptr);
    };
}  // namespace task
