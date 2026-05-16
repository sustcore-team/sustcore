/**
 * @file task.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程与线程
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/description.h>
#include <exe/task.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sus/map.h>
#include <sus/nonnull.h>
#include <sus/queue.h>
#include <task/task_struct.h>

namespace task {
    struct ForkResult {
        CapIdx child_pcb_cap;
        pid_t child_pid;
    };

    class TaskManager {
    private:
        size_t __tid_alloc = 1;
        size_t __pid_alloc = 1;

        size_t alloc_tid() {
            return __tid_alloc++;
        }

        size_t alloc_pid() {
            return __pid_alloc++;
        }

        util::LinkedMap<pid_t, PCB *> _pid_map;
        util::LinkedList<PCB *> _recycle_pcbs;

        /**
         * @brief 分配并返回一个新的 TCB 对象的非空指针.
         *
         * @return util::nonnull<TCB *> 新分配的 TCB, 所有权归调用者.
         * @note 返回的对象通过 `new` 分配, 调用者负责在合适时机释放或移交所有权.
         */
        util::nonnull<TCB *> alloc_tcb() {
            return util::nnullforce(new TCB());
        }

        /**
         * @brief 分配并返回一个新的 PCB 对象的非空指针.
         *
         * @return util::nonnull<PCB *> 新分配的 PCB, 所有权归调用者.
         * @note 返回的对象通过 `new` 分配, 调用者负责在合适时机释放或移交所有权.
         */
        util::nonnull<PCB *> alloc_pcb() {
            return util::nnullforce(new PCB());
        }

        /**
         * @brief 初始化一个 TCB 对象, 并将其与所属的 PCB 关联.
         *
         * @param tcb 要初始化的 TCB, 必须为非空指针.
         * @param task 所属的 PCB, 必须为非空指针.
         * @return Result<void> 成功返回 Ok, 失败返回相应错误码.
         * @note 调用者需保证在适当的锁或上下文中调用本函数, 并且在 TCB 被调度前完成初始化.
         */
        Result<void> init_tcb(util::nonnull<TCB *> tcb,
                      util::nonnull<PCB *> task /* ... args*/);
        /**
         * @brief 初始化任务上下文寄存器与栈, 使 TCB 可用于首次调度.
         *
         * @param tcb 目标 TCB, 必须为非空指针.
         * @param entrypoint 线程入口地址.
         * @param stack_top 线程栈顶地址.
         * @return Result<void> 成功返回 Ok, 失败返回相应错误码.
         * @note entrypoint 与 stack_top 必须满足架构对对齐和可访问性的要求.
         */
        Result<void> init_ctx(util::nonnull<TCB *> tcb, void *entrypoint,
                      void *stack_top);
        /**
         * @brief 初始化 PCB 对象, 根据给定的 TaskSpec 分配或配置资源.
         *
         * @param pcb 要初始化的 PCB, 必须为非空指针.
         * @param spec 任务规格, 包含可执行路径、权限、资源限制等信息.
         * @return Result<void> 成功返回 Ok, 失败返回相应错误码.
         * @note 本函数可能分配内存与能力(capability), 如果失败调用者应清理部分已分配的资源.
         */
        Result<void> init_pcb(util::nonnull<PCB *> pcb,
                      TaskSpec spec /* ... args*/);

        /**
         * @brief 在指定的 PCB 上创建一个线程, 并初始化其 TCB 与上下文.
         *
         * @param pcb 目标 PCB, 必须为非空指针.
         * @param entrypoint 线程入口地址.
         * @param stack_top 线程栈顶地址.
         * @param schd_class 调度器类别, 用于选择调度策略.
         * @return Result<util::nonnull<TCB *>> 成功返回新线程的 TCB 非空指针, 失败返回错误码.
         * @note 新 TCB 的生命周期由返回值的所有者负责管理, 创建后应在合适时刻加入调度队列.
         */
        Result<util::nonnull<TCB *>> construct_thread(
            util::nonnull<PCB *> pcb, void *entrypoint, void *stack_top,
            schd::ClassType schd_class);
        /**
         * @brief 为 PCB 构造主线程, 并根据 StartupInfo 执行额外的启动配置.
         *
         * @param pcb 目标 PCB, 必须为非空指针.
         * @param schd_class 调度器类别.
         * @param startup_info 启动时需要的附加信息.
         * @return Result<util::nonnull<TCB *>> 成功返回主线程的 TCB 非空指针, 失败返回错误码.
         * @note 主线程通常承担进程初始化逻辑, 应在创建后尽快加入调度.
         */
        Result<util::nonnull<TCB *>> construct_main_thread(
            util::nonnull<PCB *> pcb, schd::ClassType schd_class,
            task::StartupInfo startup_info);

        /**
         * @brief 终止并清理指定的 TCB, 包括释放与其相关的内核资源.
         *
         * @param tcb 要终止的 TCB, 必须为非空指针.
         * @return Result<void> 成功返回 Ok, 失败返回相应错误码.
         * @note 调用方需保证该 TCB 当前未被运行或已从调度器中移除, 并处理并发访问场景.
         */
        Result<void> terminate_tcb(util::nonnull<TCB *> tcb);
        /**
         * @brief 终止并清理指定的 PCB, 包括其所有线程和占用的资源.
         *
         * @param pcb 要终止的 PCB, 必须为非空指针.
         * @return Result<void> 成功返回 Ok, 失败返回相应错误码.
         * @note 本操作会尝试终止所有线程并释放资源, 调用者应当确保不会在后续继续访问该 PCB.
         */
        Result<void> terminate_pcb(util::nonnull<PCB *> pcb);

        /**
         * @brief 预加载可执行文件相关信息到 TaskSpec 与 LoadPrm, 但不创建任务.
         *
         * @param path 可执行文件路径, 以 NUL 结尾的字符串.
         * @param spec 输出的 TaskSpec, 将被填充所需信息.
         * @param prm 输出的 LoadPrm, 包含加载时的参数.
         * @return Result<void> 成功返回 Ok, 失败返回错误码.
         * @note 本函数不会启动任务, 但可能分配临时内存, 调用者在失败时应清理相关资源.
         */
        Result<void> preload(const char *path, TaskSpec &spec, LoadPrm &prm);

    public:
        /**
         * @brief 初始化 TaskManager 单例, 仅应在内核启动早期调用.
         */
        static void init();

        /**
         * @brief 返回 TaskManager 的单例引用.
         *
         * @return TaskManager& 单例引用.
         * @note 在调用本函数前应确保已经调用 `init()` 进行初始化.
         */
        static TaskManager &inst();

        /**
         * @brief 创建并返回 init 进程的 PCB, 用于系统初始化任务.
         *
         * @param spec init 进程的 TaskSpec 配置.
         * @return Result<util::nonnull<PCB *>> 成功返回 PCB 的非空指针, 失败返回错误码.
         * @note 该函数用于创建系统初始化进程, 可能在内核启动阶段被特殊处理.
         */
        Result<util::nonnull<PCB *>> create_init_task(
            TaskSpec spec /* ... args*/);
        /**
         * @brief 根据 TaskSpec 创建一个新任务, 并为其配置指定的调度类.
         *
         * @param spec 任务规格, 包含可执行文件路径、资源限制等.
         * @param schd_class 调度器类别, 用于任务调度策略选择.
         * @return Result<util::nonnull<PCB *>> 成功返回新建 PCB 的非空指针, 失败返回错误码.
         * @note 该函数可能会分配能力(capability)与内存, 调用者应检查返回的错误并在必要时回滚.
         */
        Result<util::nonnull<PCB *>> create_task(
            TaskSpec spec, schd::ClassType schd_class /* ... args*/);

        /**
         * @brief 查找指定 pid 的 holder id, 用于能力索引或权限校验.
         *
         * @param pid 要查找的进程 id.
         * @return Result<size_t> 成功返回 holder id, 失败返回错误码.
         */
        Result<size_t> lookup_holder_id(pid_t pid);
        /**
         * @brief 将当前进程进行 fork, 创建子进程并返回子进程相关信息.
         *
         * @return Result<ForkResult> 成功返回子进程的能力索引与 pid, 失败返回错误码.
         * @note fork 操作可能涉及写时复制与资源共享, 调用者需注意并发与内存一致性问题.
         */
        Result<ForkResult> fork_current();
        /**
         * @brief 终止当前进程, 并在必要时回收其资源或加入回收队列.
         *
         * @return Result<void> 成功返回 Ok, 失败返回错误码.
         * @note 本函数可能不会立即释放所有资源, 某些清理工作会异步进行或由 `reap_recycled` 完成.
         */
        Result<void> exit_current();
        /**
         * @brief 将已完成的 PCB 加入回收队列, 以便稍后统一回收.
         *
         * @param pcb 要回收的 PCB 指针, 可以为 nullptr 时忽略.
         * @note 此接口应保证线程安全, 并避免在回收前继续访问该 PCB.
         */
        void enqueue_recycle(PCB *pcb);
        /**
         * @brief 处理回收队列, 释放已经入队的 PCB 及其资源.
         *
         * @note 该函数可能在内核空闲时或专门的回收线程中调用, 需要保证不会与其他使用 PCB 的操作发生竞态.
         */
        void reap_recycled();

        /**
         * @brief 加载 ELF 可执行文件并创建对应的 PCB, 设置指定的调度类.
         *
         * @param path 可执行文件路径.
         * @param schd_class 指定的调度器类别.
         * @return Result<util::nonnull<PCB *>> 成功返回 PCB 的非空指针, 失败返回错误码.
         * @note 该函数会解析 ELF 并分配必要资源, 调用者需在错误情况下负责清理.
         */
        Result<util::nonnull<PCB *>> load_elf(const char *path,
                              schd::ClassType schd_class);
        /**
         * @brief 加载并创建 init 进程的 PCB, 路径通常指向系统初始化程序.
         *
         * @param path init 可执行文件路径.
         * @return Result<util::nonnull<PCB *>> 成功返回 init 的 PCB 非空指针, 失败返回错误码.
         */
        Result<util::nonnull<PCB *>> load_init(const char *path);
    };

    void init_kop();
}  // namespace task
