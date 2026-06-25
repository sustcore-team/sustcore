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
#include <sustcore/bootstrap.h>
#include <schd/schdbase.h>
#include <sus/list.h>
#include <sus/map.h>
#include <sus/nonnull.h>
#include <task/task_struct.h>

#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace task {
    struct ForkResult {
        CapIdx child_pcb_cap;
        pid_t child_pid;
    };

    struct UserInitLayout {
        VirAddr sp;
        size_t argc;
        VirAddr argv;
        VirAddr envp;
        VirAddr auxv;
    };

    [[nodiscard]]
    wait::wd_t task_exit_wait_wd() noexcept;

    class TaskManager {
    private:
        std::atomic<size_t> __tid_alloc = 1;
        std::atomic<size_t> __pid_alloc = 1;

        size_t alloc_tid() {
            return __tid_alloc++;
        }

        size_t alloc_pid() {
            return __pid_alloc++;
        }

        std::unordered_map<pid_t, PCB *> _pid_map;
        util::LinkedList<PCB *> _recycle_pcbs;
        PCB *_kernel_pcb = nullptr;

        /**
         * @brief 分配并返回一个新的 TCB 对象的非空指针.
         *
         * @return util::nonnull<TCB *> 新分配的 TCB, 所有权归调用者.
         * @note 返回的对象通过 `new` 分配,
         * 调用者负责在合适时机释放或移交所有权.
         */
        util::nonnull<TCB *> alloc_tcb() {
            TCB *tcb                  = new TCB();
            tcb->tid                  = 0;
            tcb->task                 = nullptr;
            tcb->is_kernel            = false;
            tcb->kentry               = nullptr;
            tcb->karg                 = nullptr;
            tcb->list_head            = {};
            tcb->kstack_bottom        = nullptr;
            tcb->ksp                  = nullptr;
            tcb->kstack_phy           = PhyAddr::null;
            tcb->schd_class           = schd::ClassType::BOT;
            tcb->basic_entity.state   = ThreadState::EMPTY;
            tcb->basic_entity.rq_head = {};
            tcb->rr_entity            = {};
            tcb->wait_wd              = 0;
            tcb->wait_predicate       = {};
            tcb->syscall_info.reset();
            tcb->wait_head = {};
            return util::nnullforce(tcb);
        }

        /**
         * @brief 分配并返回一个新的 PCB 对象的非空指针.
         *
         * @return util::nonnull<PCB *> 新分配的 PCB, 所有权归调用者.
         * @note 返回的对象通过 `new` 分配,
         * 调用者负责在合适时机释放或移交所有权.
         */
        util::nonnull<PCB *> alloc_pcb() {
            return util::nnullforce(new PCB());
        }

        /**
         * @brief 初始化一个 TCB 对象, 并将其与所属的 PCB 关联.
         *
         * @param tcb 要初始化的 TCB, 必须为非空指针.
         * @param task 所属的 PCB, 必须为非空指针.
         * @return Result<void> 成功返回 SUCCESS, 失败返回相应错误码.
         * @note 调用者需保证在适当的锁或上下文中调用本函数, 并且在 TCB
         * 被调度前完成初始化.
         */
        Result<void> init_tcb(util::nonnull<TCB *> tcb,
                              util::nonnull<PCB *> task /* ... args*/);
        Result<void> recycle_tcb(util::nonnull<TCB *> tcb);
        /**
         * @brief 分配一个新的 TCB 并完成与所属 PCB 的基础绑定.
         *
         * @param pcb 所属 PCB.
         * @return 成功返回已完成基础初始化的 TCB.
         */
        Result<util::nonnull<TCB *>> create_bound_tcb(util::nonnull<PCB *> pcb);
        /**
         * @brief 初始化一个空 PCB, 只填入生命周期与进程基础状态.
         *
         * @param pcb 要初始化的 PCB, 必须为非空指针.
         * @return Result<void> 成功返回 SUCCESS, 失败返回相应错误码.
         * @note 资源字段由后续的填充函数写入.
         */
        Result<void> init_pcb(util::nonnull<PCB *> pcb);

        /**
         * @brief 在指定的 PCB 上创建一个线程, 并初始化其 TCB 与上下文.
         *
         * @param pcb 目标 PCB, 必须为非空指针.
         * @param entrypoint 线程入口地址.
         * @param stack_top 线程栈顶地址.
         * @param schd_class 调度器类别, 用于选择调度策略.
         * @return Result<util::nonnull<TCB *>> 成功返回新线程的 TCB 非空指针,
         * 失败返回错误码.
         * @note 新 TCB 的生命周期由返回值的所有者负责管理,
         * 创建后应在合适时刻加入调度队列.
         */
        Result<util::nonnull<TCB *>> construct_thread(
            util::nonnull<PCB *> pcb, void *entrypoint, void *stack_top,
            schd::ClassType schd_class, bool kernel_thread = false);
        /**
         * @brief 将 Linux 风格启动布局写入用户主栈并返回新的初始栈顶.
         *
         * @param stack_mem 主栈对应的 MemoryPayload.
         * @param stack_top 用户栈顶虚拟地址.
         * @param spec 包含 argv/envp/auxv/bsargv 的启动信息.
         * @return 成功返回新的用户态初始栈顶地址.
         */
        [[nodiscard]]
        Result<UserInitLayout> build_user_stack(cap::MemoryPayload &stack_mem,
                                                VirAddr stack_top,
                                                TaskSpec &spec, CapIdx pcb_cap,
                                                CapIdx main_tcb_cap,
                                                CapIdx stack_mem_cap);

        /**
         * @brief 根据 TaskSpec 填充 PCB 并构造主线程.
         *
         * @param pcb 目标 PCB, 必须为非空指针, 用于承载新任务的状态与资源.
         * @param spec 任务规格, 包含加载后的地址空间、能力空间和入口点.
         * @param schd_class 调度器类别, 用于选择调度策略.
         * @param reuse_main_tcb 可选的 TCB 指针, 如果非空则重用该 TCB
         * 作为主线程, 否则新分配一个 TCB.
         * @return 成功返回主线程 TCB, 失败返回相应错误码.
         */
        Result<util::nonnull<TCB *>> populate_task(
            util::nonnull<PCB *> pcb, TaskSpec spec, schd::ClassType schd_class,
            TCB *reuse_main_tcb = nullptr);
        /**
         * @brief 为给定用户线程补齐主线程共享资源并完成首次用户态切入准备.
         */
        Result<util::nonnull<TCB *>> setup_user_main_thread(
            util::nonnull<PCB *> pcb, util::nonnull<TCB *> tcb, TaskSpec &spec,
            bool link_into_pcb, const char *log_tag);
        /**
         * @brief 创建一个已完成装载的用户进程, 可选择是否立即唤醒主线程.
         */
        Result<util::nonnull<PCB *>> create_loaded_user_task(
            TaskSpec spec, schd::ClassType schd_class, bool wakeup);
        /**
         * @brief 将已装载好的 TaskSpec 提交为正式用户进程.
         *
         * @param spec 已完成装载但尚未移交所有权的任务规格.
         * @param schd_class 主线程调度类别.
         * @param wakeup 是否立即唤醒主线程.
         * @return 创建成功的 PCB.
         */
        Result<util::nonnull<PCB *>> commit_loaded_task(
            TaskSpec spec, schd::ClassType schd_class, bool wakeup);

        /**
         * @brief 使用 fork 语义填充已初始化的子 PCB.
         *
         * @param child_pcb 空子 PCB.
         * @param parent_pcb 父 PCB.
         * @param parent_tcb 当前父线程.
         * @param parent_ctx fork 入口处保存的父线程用户上下文.
         * @param ret_slot 父子进程中存放子 PCB capability 的槽位.
         * @return 成功返回子进程主线程 TCB, 失败返回错误码.
         */
        Result<util::nonnull<TCB *>> populate_forked_task(
            util::nonnull<PCB *> child_pcb, PCB *parent_pcb, TCB *parent_tcb,
            Context *parent_ctx, CapIdx ret_slot);

        /**
         * @brief 获取唯一 KERNEL 进程 PCB.
         */
        Result<util::nonnull<PCB *>> kernel_pcb();

        /**
         * @brief 终止并清理指定的 PCB, 包括其所有线程和占用的资源.
         *
         * @param pcb 要终止的 PCB, 必须为非空指针.
         * @return Result<void> 成功返回 SUCCESS, 失败返回相应错误码.
         * @note 本操作会尝试终止所有线程并释放资源,
         * 调用者应当确保不会在后续继续访问该 PCB.
         */
        Result<void> terminate_pcb(util::nonnull<PCB *> pcb);

        /**
         * @brief 预加载可执行文件相关信息到 TaskSpec 与 LoadPrm, 但不创建任务.
         *
         * @param image_cap 可执行文件能力.
         * @param spec 输出的 TaskSpec, 将被填充所需信息.
         * @param prm 输出的 LoadPrm, 包含加载时的参数.
         * @return Result<void> 成功返回 SUCCESS, 失败返回错误码.
         * @return Image File 的能力索引, 成功返回该索引, 失败返回相应错误码.
         */
        Result<CapIdx> preload(CapIdx image_cap, TaskSpec &spec, LoadPrm &prm);

        /**
         * @brief 预加载可执行文件相关信息到指定的 TaskSpec, LoadPrm 与 holder
         *
         * @param image_cap 可执行文件能力.
         * @param holder 能力持有者, 用于执行能力移除操作.
         * @param spec 输出的 TaskSpec, 将被填充所需信息.
         * @param prm 输出的 LoadPrm, 包含加载时的参数.
         * @return Image File 的能力索引, 成功返回该索引, 失败返回相应错误码.
         */
        Result<CapIdx> preload_into(CapIdx image_cap, cap::CHolder *holder,
                                    TaskSpec &spec, LoadPrm &prm);
        [[nodiscard]]
        Result<void> validate_bootstrap_record(
            const TaskSpec::BootstrapRecordData &record) noexcept;
        [[nodiscard]]
        Result<void> append_bootstrap_cap_explain_record(
            TaskSpec &spec, CapIdx cap_idx, PayloadType cap_type, b64 cap_perm,
            const char *cap_desc);
        [[nodiscard]]
        Result<void> append_bootstrap_vaddr_explain_record(
            TaskSpec &spec, VirAddr vaddr, const char *vaddr_desc);
        [[nodiscard]]
        Result<void> append_bootstrap_path_explain_record(
            TaskSpec &spec, const char *path_desc);
        /**
         * @brief 复制启动缓冲区、预加载并装载 ELF 到 TaskSpec.
         */
        Result<void> load_task_spec(CapIdx image_cap, cap::CHolder *holder,
                                    const std::vector<std::string> &argv,
                                    const std::vector<std::string> &envp,
                                    const std::vector<
                                        TaskSpec::BootstrapRecordData> &bsargv,
                                    TaskSpec &spec, LoadPrm &prm);
        Result<void> load_linux_task_spec(CapIdx image_cap,
                                          cap::CHolder *holder,
                                          CapIdx subsystem_image_cap,
                                          const std::vector<std::string> &argv,
                                          const std::vector<std::string> &envp,
                                          const std::vector<
                                              TaskSpec::BootstrapRecordData>
                                              &bsargv,
                                          TaskSpec &spec, LoadPrm &prm);
        /**
         * @brief 装载 ELF 并直接构造对应的用户进程.
         */
        Result<util::nonnull<PCB *>> load_task_image(
            CapIdx image_cap, cap::CHolder *holder, schd::ClassType schd_class,
            bool wakeup, const std::vector<std::string> &argv = {},
            const std::vector<std::string> &envp = {},
            const std::vector<TaskSpec::BootstrapRecordData> &bsargv = {});
        Result<util::nonnull<PCB *>> load_linux_task_image(
            CapIdx image_cap, cap::CHolder *holder, CapIdx subsystem_image_cap,
            schd::ClassType schd_class, bool wakeup,
            const std::vector<std::string> &argv = {},
            const std::vector<std::string> &envp = {},
            const std::vector<TaskSpec::BootstrapRecordData> &bsargv = {});

    public:
        /**
         * @brief 初始化 TaskManager 单例, 仅应在内核启动早期调用.
         */
        static void init();
        static bool initialized();

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
         * @return Result<util::nonnull<PCB *>> 成功返回 PCB 的非空指针,
         * 失败返回错误码.
         * @note 该函数用于创建系统初始化进程, 可能在内核启动阶段被特殊处理.
         */
        Result<util::nonnull<PCB *>> create_init_task(
            TaskSpec spec /* ... args*/);
        /**
         * @brief 根据 TaskSpec 创建一个新任务, 并为其配置指定的调度类.
         *
         * @param spec 任务规格, 包含可执行文件路径、资源限制等.
         * @param schd_class 调度器类别, 用于任务调度策略选择.
         * @return Result<util::nonnull<PCB *>> 成功返回新建 PCB 的非空指针,
         * 失败返回错误码.
         * @note 该函数可能会分配能力(capability)与内存,
         * 调用者应检查返回的错误并在必要时回滚.
         */
        Result<util::nonnull<PCB *>> create_task(
            TaskSpec spec, schd::ClassType schd_class /* ... args*/);

        /**
         * @brief 创建唯一 KERNEL 进程.
         */
        Result<util::nonnull<PCB *>> create_kernel_task();

        /**
         * @brief 在 KERNEL 进程中创建内核线程.
         */
        Result<util::nonnull<TCB *>> create_kernel_thread(
            void (*entry)(), schd::ClassType schd_class);
        Result<util::nonnull<TCB *>> create_kernel_thread(
            KThreadEntry entry, void *arg, schd::ClassType schd_class);

        /**
         * @brief 创建第一个内核 idle 线程.
         */
        Result<util::nonnull<TCB *>> create_idle_thread();

        /**
         * @brief 创建启动期 kinit 内核线程.
         */
        Result<util::nonnull<TCB *>> create_kinit_thread();

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
         * @return Result<ForkResult> 成功返回子进程的能力索引与 pid,
         * 失败返回错误码.
         * @note fork 操作可能涉及写时复制与资源共享,
         * 调用者需注意并发与内存一致性问题.
         */
        Result<ForkResult> fork_current(CapIdx ret_slot);

        /**
         * @brief 为当前进程创建一个线程, 线程入口与栈由参数指定.
         *
         * @param entry 线程入口地址.
         * @param stack_addr 线程栈地址.
         * @param stack_size 线程栈大小.
         * @return Result<CapIdx> 返回新线程的 TCB 能力索引.
         */
        Result<CapIdx> create_thread_current(VirAddr entry, VirAddr stack_addr,
                                             size_t stack_size);
        /**
         * @brief 用指定ELF替换当前进程镜像, 并保留指定能力.
         *
         * @param image_cap 新程序文件能力.
         * @param reserved_caps 需要保留的能力槽位列表, 可为空.
         * @param reserved_count reserved_caps 中的元素数量.
         * @return Result<void> 成功不返回旧用户镜像, 失败保持当前进程不变.
         */
        Result<void> exec_current(CapIdx image_cap, const CapIdx *reserved_caps,
                                  size_t reserved_count);
        /**
         * @brief 用指定ELF替换目标 PCB 的进程镜像.
         *
         * 目标为当前进程时复用当前 TCB; 目标为其他进程时移除旧线程并重建主线程.
         */
        Result<void> exec_pcb(util::nonnull<PCB *> pcb, CapIdx image_cap,
                              const CapIdx *reserved_caps,
                              size_t reserved_count,
                              const std::vector<std::string> &argv = {},
                              const std::vector<std::string> &envp = {},
                              const std::vector<TaskSpec::BootstrapRecordData>
                                  &bsargv = {});
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
         * @note 该函数可能在内核空闲时或专门的回收线程中调用,
         * 需要保证不会与其他使用 PCB 的操作发生竞态.
         */
        void reap_recycled();

        /**
         * @brief 加载 ELF 可执行文件并创建对应的 PCB, 设置指定的调度类.
         *
         * @param image_cap 可执行文件能力.
         * @param schd_class 指定的调度器类别.
         * @return Result<util::nonnull<PCB *>> 成功返回 PCB 的非空指针,
         * 失败返回错误码.
         * @note 该函数会解析 ELF 并分配必要资源, 调用者需在错误情况下负责清理.
         */
        Result<util::nonnull<PCB *>> load_elf(CapIdx image_cap,
                                              schd::ClassType schd_class);
        /**
         * @brief 使用调用方提供的 CHolder 加载 ELF 并创建进程.
         *
         * 该接口不会创建新的 CHolder; 调用方应先完成需要继承或注入的
         * capability 配置, 再把 holder 传入. ELF image、heap/stack Memory、
         * PCB/TCB capability 会在该 holder 的空闲槽中继续创建.
         *
         * @param image_cap 可执行文件能力.
         * @param holder 预先构造并配置好的进程 CHolder.
         * @param schd_class 调度类别.
         * @return 创建成功的 PCB.
         */
        Result<util::nonnull<PCB *>> load_elf_into(
            CapIdx image_cap, cap::CHolder *holder, schd::ClassType schd_class,
            const std::vector<std::string> &argv = {},
            const std::vector<std::string> &envp = {},
            const std::vector<TaskSpec::BootstrapRecordData> &bsargv = {});
        Result<util::nonnull<PCB *>> load_linux_elf_into(
            CapIdx image_cap, cap::CHolder *holder, CapIdx subsystem_image_cap,
            schd::ClassType schd_class, const std::vector<std::string> &argv = {},
            const std::vector<std::string> &envp = {},
            const std::vector<TaskSpec::BootstrapRecordData> &bsargv = {});
        /**
         * @brief 加载并创建 init 进程的 PCB, 路径通常指向系统初始化程序.
         *
         * @param path init 可执行文件路径.
         * @return Result<util::nonnull<PCB *>> 成功返回 init 的 PCB 非空指针,
         * 失败返回错误码.
         */
        Result<util::nonnull<PCB *>> load_init(const char *path);
    };

    void init_kop();
}  // namespace task
