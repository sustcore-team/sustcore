/**
 * @file task.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 进程/线程
 * @version alpha-1.0.0
 * @date 2026-01-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <cap/cholder.h>
#include <cap/permission.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/callconv.h>
#endif
#include <env.h>
#include <exe/elfloader.h>
#include <exe/task.h>
#include <guard.h>
#include <kinit.h>
#include <kio.h>
#include <kmod/bootstrap.h>
#include <logger.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <mem/vma.h>
#include <object/memory.h>
#include <object/task.h>
#include <schd/rr.h>
#include <schd/schdbase.h>
#include <sus/nonnull.h>
#include <sus/raii.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>
#include <task/scheduler.h>
#include <task/startup.h>
#include <task/task.h>
#include <task/task_struct.h>
#include <task/wait.h>
#include <vfs/vfs.h>

#include <cassert>
#include <cstring>
#include <vector>

namespace task {
    namespace {
        extern "C" [[noreturn]] void new_utask_trampoline();

        void init_user_context(Context *ctx, void *entrypoint, void *stack_top,
                               void *kstack_top) noexcept {
            assert(ctx != nullptr);
            *ctx = {};
            ctx->setup_regs(false, false, true);
            ctx->pc()         = reinterpret_cast<umb_t>(entrypoint);
            ctx->sp()         = reinterpret_cast<umb_t>(stack_top);
            ctx->kstack_top() = reinterpret_cast<umb_t>(kstack_top);
        }

        void init_kernel_context(Context *ctx, void *entrypoint, void *arg0,
                                 void *stack_top, bool sie,
                                 bool spie) noexcept {
            assert(ctx != nullptr);
            *ctx = {};
            ctx->setup_regs(true, sie, spie);
            ctx->sp() = reinterpret_cast<umb_t>(stack_top);
            ctx->ra   = reinterpret_cast<umb_t>(entrypoint);
            ctx->s0   = reinterpret_cast<umb_t>(arg0);
        }

        void reset_thread_runtime(util::nonnull<TCB *> tcb) noexcept {
            tcb->basic_entity.state   = ThreadState::EMPTY;
            tcb->basic_entity.rq_head = {};
            tcb->basic_entity.flags   = 0;
            tcb->rr_entity            = {};
            tcb->syscall_info.reset();
        }

        void build_user_contexts(util::nonnull<TCB *> tcb, void *entrypoint,
                                 void *user_stack_top) noexcept {
            tcb->reset_kstack();
            auto *user_ctx = tcb->push<Context>();
            init_user_context(user_ctx, entrypoint, user_stack_top,
                              tcb->kstack_top());
            init_kernel_context(tcb->kernel_context_ptr(),
                                reinterpret_cast<void *>(&new_utask_trampoline),
                                user_ctx, tcb->kstack_top(), false, true);
        }

        void build_kernel_context(util::nonnull<TCB *> tcb, void *entrypoint,
                                  void *arg0) noexcept {
            tcb->reset_kstack();
            init_kernel_context(tcb->kernel_context_ptr(), entrypoint, arg0,
                                tcb->kstack_top(), true, false);
        }
    }  // namespace

    void TCB::SyscallInfo::reset() noexcept {
        syscall_args   = {};
        syscall_number = 0;
        syscall_result = {};
        syscall_state  = State::NONE;
    }

    void TCB::SyscallInfo::begin(const syscall::ArgPack &args) noexcept {
        reset();
        syscall_args   = args;
        syscall_number = args.syscall_number;
        syscall_state  = State::EXECUTING;
    }

    void TCB::SyscallInfo::complete(const syscall::RetPack &ret) noexcept {
        syscall_result = ret;
        syscall_state  = State::COMPLETED;
    }

    namespace {
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
        TaskManager inst_task_manager;
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
        bool inst_task_manager_initialized = false;

        /**
         * @brief 为 PCB 在自身 CHolder 中插入 PCB capability.
         *
         * @param pcb 需要暴露 capability 的 PCB.
         * @return 成功返回插入的 capability 槽位, 失败返回错误码.
         */
        Result<CapIdx> insert_pcb_cap(PCB *pcb) {
            if (pcb == nullptr || pcb->cholder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto *payload = new cap::PCBPayload(pcb);
            if (payload == nullptr) {
                unexpect_return(ErrCode::OUT_OF_MEMORY);
            }
            auto insert_res = pcb->cholder->insert_to_free(payload);
            if (!insert_res.has_value()) {
                delete payload;
                propagate_return(insert_res);
            }
            return insert_res.value();
        }

        /**
         * @brief 检查指定的能力索引列表中是否包含特定索引, 用于验证 exec
         * 时保留的能力是否合法.
         *
         * @param idx 要检查的能力索引.
         * @param list 能力索引列表, 可能包含重复项.
         * @param count list 中的元素数量.
         * @return true 如果 idx 在 list 中出现过, 否则返回 false.
         * @return false 如果 idx 不在 list 中出现过.
         */
        bool capidx_in_list(CapIdx idx, const CapIdx *list, size_t count) {
            for (size_t i = 0; i < count; ++i) {
                if (list[i] == idx) {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 验证 reserved caps 是否合法
         *
         * @param holder 能力持有者, 用于查询能力信息.
         * @param pcb_cap PCB 的能力索引, 必须有效.
         * @param reserved_caps 需要保留的能力索引列表, 每个索引必须有效.
         * @param reserved_count reserved_caps 中的元素数量.
         * @return Result<void> 成功返回 SUCCESS, 失败返回相应错误码.
         */
        Result<void> verify_reserved_caps(cap::CHolder *holder, CapIdx pcb_cap,
                                          const CapIdx *reserved_caps,
                                          size_t reserved_count) {
            if (holder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            auto pcb_cap_res = holder->lookup(pcb_cap);
            propagate(pcb_cap_res);
            for (size_t i = 0; i < reserved_count; ++i) {
                CapIdx idx   = reserved_caps[i];
                auto cap_res = holder->lookup(idx);
                propagate(cap_res);
            }
            void_return();
        }

        /**
         * @brief 移除 holder 中除了 pcb_cap 和 reserved_caps
         * 列表中索引以外的所有能力, 用于 exec
         * 替换镜像时清理不需要保留的能力槽位.
         *
         * @param holder 能力持有者, 用于执行能力移除操作.
         * @param pcb_cap PCB 的能力索引, 需要保留.
         * @param reserved_caps 需要保留的能力索引列表, 每个索引都需要保留.
         * @param reserved_count reserved_caps 中的元素数量.
         * @return Result<void> 成功返回 SUCCESS, 失败返回相应错误码.
         */
        Result<void> remove_unreserved_caps(cap::CHolder *holder,
                                            CapIdx pcb_cap,
                                            const CapIdx *reserved_caps,
                                            size_t reserved_count) {
            if (holder == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            ErrCode err = ErrCode::SUCCESS;
            holder->space().foreach ([&](CapIdx idx, cap::Capability *) {
                if (err != ErrCode::SUCCESS) {
                    return;
                }
                if (idx == pcb_cap ||
                    capidx_in_list(idx, reserved_caps, reserved_count))
                {
                    return;
                }
                auto remove_res = holder->remove(idx);
                if (!remove_res.has_value()) {
                    err = remove_res.error();
                }
            });
            if (err != ErrCode::SUCCESS) {
                unexpect_return(err);
            }
            void_return();
        }

        /**
         * @brief 构造空的 TaskSpec, 供 ELF preload/load 填充.
         *
         * @return 字段均为空值的 TaskSpec.
         */
        TaskSpec empty_task_spec() {
            return TaskSpec{
                .tmm          = util::owner<TaskMemoryManager *>(nullptr),
                .holder       = nullptr,
                .entrypoint   = VirAddr(static_cast<addr_t>(0)),
                .startup_blob = util::owner<char *>(nullptr),
            };
        }

        [[nodiscard]]
        bool valid_bootstrap_cap_path(const BootstrapCapPathView &record) {
            return record.cap != cap::null && record.cap != cap::error &&
                   record.path != nullptr && record.path[0] == '/';
        }

        [[nodiscard]]
        size_t bootstrap_cap_path_record_size(
            const BootstrapCapPathView &record) {
            return sizeof(BootstrapRecordHeader) + sizeof(CapIdx) +
                   strlen(record.path) + 1;
        }

        [[nodiscard]]
        Result<void> append_bootstrap_record(char *dst, size_t blob_size,
                                             size_t &offset, uint32_t type,
                                             const void *data,
                                             size_t data_size) {
            if (dst == nullptr || data == nullptr) {
                unexpect_return(ErrCode::NULLPTR);
            }
            size_t record_size = sizeof(BootstrapRecordHeader) + data_size;
            if (offset > blob_size || blob_size - offset < record_size) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            auto *header =
                reinterpret_cast<BootstrapRecordHeader *>(dst + offset);
            header->next = 0;
            header->type = type;
            memcpy(dst + offset + sizeof(BootstrapRecordHeader), data,
                   data_size);
            offset += record_size;
            void_return();
        }

        [[nodiscard]]
        Result<void> append_bootstrap_cap_path_record(
            char *dst, size_t blob_size, size_t &offset, uint32_t type,
            const BootstrapCapPathView &record) {
            if (!valid_bootstrap_cap_path(record)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }

            size_t path_len = strlen(record.path) + 1;
            size_t record_size =
                sizeof(BootstrapRecordHeader) + sizeof(CapIdx) + path_len;
            if (offset > blob_size || blob_size - offset < record_size) {
                unexpect_return(ErrCode::OUT_OF_BOUNDARY);
            }

            auto *header =
                reinterpret_cast<BootstrapRecordHeader *>(dst + offset);
            header->next = 0;
            header->type = type;
            memcpy(dst + offset + sizeof(BootstrapRecordHeader), &record.cap,
                   sizeof(record.cap));
            memcpy(dst + offset + sizeof(BootstrapRecordHeader) +
                       sizeof(record.cap),
                   record.path, path_len);
            offset += record_size;
            void_return();
        }

        void finalize_bootstrap_record_next(
            char *blob, const std::vector<size_t> &offsets) {
            for (size_t i = 0; i < offsets.size(); ++i) {
                auto *header = reinterpret_cast<BootstrapRecordHeader *>(
                    blob + offsets[i]);
                header->next = i + 1 < offsets.size() ? offsets[i + 1] : 0;
            }
        }

        /**
         * @brief 释放 TaskSpec 中尚未移交给 PCB 的 TaskMemoryManager.
         *
         * @param spec 待清理的 TaskSpec.
         */
        void destroy_unowned_task_memory(TaskSpec &spec) {
            if (spec.tmm.get() == nullptr) {
                if (spec.startup_blob != nullptr) {
                    delete[] spec.startup_blob.get();
                    spec.startup_blob      = util::owner<char *>(nullptr);
                    spec.startup_blob_size = 0;
                }
                return;
            }
            PhyAddr pgd = spec.tmm->pgd();
            delete spec.tmm;
            GFP::put_page(pgd, 1);
            spec.tmm = util::owner<TaskMemoryManager *>(nullptr);
            if (spec.startup_blob != nullptr) {
                delete[] spec.startup_blob.get();
                spec.startup_blob      = util::owner<char *>(nullptr);
                spec.startup_blob_size = 0;
            }
        }

        /**
         * @brief 从已有线程中选出 exec 后应使用的调度类别.
         *
         * @param pcb 被 exec 的进程.
         * @param current_tcb 当前线程.
         * @param target_current target 是否为当前进程.
         * @return exec 后主线程使用的调度类别.
         */
        schd::ClassType exec_sched_class(PCB *pcb, TCB *current_tcb,
                                         bool target_current) {
            if (target_current && current_tcb != nullptr) {
                return current_tcb->schd_class;
            }
            if (pcb != nullptr && !pcb->threads.empty()) {
                schd::ClassType cls = pcb->threads.front().schd_class;
                if (cls != schd::ClassType::IDLE && cls != schd::ClassType::BOT)
                {
                    return cls;
                }
            }
            return schd::ClassType::FCFS;
        }

        // 内核 idle 线程
        void kthread_idle() {
            while (true) {
                Idle::idle();
            }
        }

        [[noreturn]]
        void kthread_exit() {
            auto *tcb = schd::Scheduler::inst().current_tcb();
            if (tcb == nullptr || !tcb->is_kernel) {
                panic("非法内核线程退出");
            }
            tcb->basic_entity.state = ThreadState::WAITING;
            tcb->basic_entity
                .template flags_set<schd::SchedMeta::FLAGS_NEED_RESCHED>();
            schd::Scheduler::inst().schedule();
            panic("内核线程退出后仍被调度");
            while (true);
        }

        [[noreturn]]
        void kthread_trampoline() {
            auto *tcb = schd::Scheduler::inst().current_tcb();
            if (tcb == nullptr || !tcb->is_kernel || tcb->kentry == nullptr) {
                panic("内核线程入口无效");
            }
            tcb->kentry(tcb->karg);
            kthread_exit();
        }

        void kthread_noarg_entry(void *arg) {
            auto entry = reinterpret_cast<void (*)()>(arg);
            entry();
        }
    }  // namespace

    void TaskManager::init() {
        // call the constructor explicitly to ensure the instance is initialized
        // before use
        new (&inst_task_manager) TaskManager();
        inst_task_manager_initialized = true;
    }

    bool TaskManager::initialized() {
        return inst_task_manager_initialized;
    }

    TaskManager &TaskManager::inst() {
        if (!initialized()) {
            panic("TaskManager 未初始化!");
        }
        return inst_task_manager;
    }

    Result<void> TaskManager::init_tcb(
        util::nonnull<TCB *> tcb, util::nonnull<PCB *> task /* ... args*/) {
        // 初始化 TCB 基本信息
        tcb->tid            = alloc_tid();
        tcb->task           = task;
        tcb->kentry         = nullptr;
        tcb->karg           = nullptr;
        tcb->list_head      = {};
        tcb->boot_role      = BootThreadRole::NONE;
        tcb->wait_wd        = 0;
        tcb->wait_predicate = {};
        tcb->wait_head      = {};
        tcb->syscall_info.reset();

        // ask for a kstack for this thread
        Result<PhyAddr> gfp_res = GFP::get_free_page(TCB::KSTACK_PAGES);
        propagate(gfp_res);

        // calculate the top of the kernel stack
        tcb->kstack_phy    = gfp_res.value() + TCB::KSTACK_PAGES * PAGESIZE;
        tcb->kstack_bottom = convert<KpaAddr>(tcb->kstack_phy).addr();
        tcb->ksp           = (char *)tcb->kstack_bottom;

        // initialization the kstack and context
        void_return();
    }

    Result<void> TaskManager::init_pcb(util::nonnull<PCB *> pcb) {
        pcb->pid            = alloc_pid();
        pcb->is_kernel      = false;
        pcb->exit_code      = 0;
        pcb->threads        = {};
        pcb->exiting        = false;
        pcb->recycle_queued = false;

        // 资源字段稍后由具体的进程创建流程填入.
        pcb->tmm          = util::owner<TaskMemoryManager *>(nullptr);
        pcb->cholder      = nullptr;
        pcb->entrypoint   = VirAddr(static_cast<addr_t>(0));
        pcb->pcb_cap      = cap::null;
        pcb->main_tcb_cap = cap::null;
        void_return();
    }

    Result<util::nonnull<TCB *>> TaskManager::construct_thread(
        util::nonnull<PCB *> pcb, void *entrypoint, void *stack_top,
        schd::ClassType schd_class, bool kernel_thread) {
        util::nonnull<TCB *> tcb = alloc_tcb();
        auto tcb_guard = util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });
        auto init_res  = init_tcb(tcb, pcb);
        if (!init_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化TCB失败! 错误码: %s",
                                     to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        tcb->is_kernel = kernel_thread;
        reset_thread_runtime(tcb);
        if (kernel_thread) {
            build_kernel_context(tcb, entrypoint, nullptr);
        } else {
            build_user_contexts(tcb, entrypoint, stack_top);
        }

        tcb->boot_role  = BootThreadRole::NONE;
        tcb->schd_class = schd_class;

        // 将线程加入进程的线程列表
        pcb->threads.push_back(*tcb);
        tcb_guard.release();  // 线程已成功构造, 释放TCB的自动释放机制
        return tcb;
    }

    Result<VirAddr> TaskManager::build_user_stack(
        cap::MemoryPayload &stack_mem, VirAddr stack_top,
        const task::StartupInfo &startup_info, const void *startup_blob,
        size_t startup_blob_size) {
        if (!stack_top.nonnull()) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (startup_blob_size != 0 && startup_blob == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        constexpr size_t STACK_ALIGN = 16;
        addr_t stack_arith           = stack_top.arith();
        size_t total_size =
            sizeof(size_t) + sizeof(task::StartupInfo) + startup_blob_size;
        if (stack_arith < total_size) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        addr_t raw_sp     = stack_arith - total_size;
        addr_t aligned_sp = raw_sp & ~(static_cast<addr_t>(STACK_ALIGN - 1));
        if (aligned_sp < USER_STACK_BOTTOM.arith()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }

        VirAddr user_sp(aligned_sp);
        size_t mem_offset = user_sp - USER_STACK_BOTTOM;
        auto size_write_res =
            stack_mem.write(mem_offset, &total_size, sizeof(total_size));
        propagate(size_write_res);
        auto info_write_res = stack_mem.write(
            mem_offset + sizeof(size_t), &startup_info, sizeof(startup_info));
        propagate(info_write_res);
        if (startup_blob_size != 0) {
            auto blob_write_res = stack_mem.write(
                mem_offset + sizeof(size_t) + sizeof(startup_info),
                startup_blob, startup_blob_size);
            propagate(blob_write_res);
        }
        loggers::TASK::DEBUG(
            "build_user_stack: stack_top=%p raw_sp=%p aligned_sp=%p "
            "mem_off=%lu total=%lu blob=%lu",
            stack_top.addr(), reinterpret_cast<void *>(raw_sp), user_sp.addr(),
            mem_offset, total_size, startup_blob_size);
        return user_sp;
    }

    Result<util::nonnull<TCB *>> TaskManager::setup_user_main_thread(
        util::nonnull<PCB *> pcb, util::nonnull<TCB *> tcb, TaskSpec &spec,
        task::StartupInfo startup_info, bool link_into_pcb,
        const char *log_tag) {
        if (pcb->cholder == nullptr || pcb->tmm.get() == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto *stack_mem = new cap::MemoryPayload(MAX_INITIAL_STACK_SIZE, false,
                                                 false, VMA::Growth::GROW_DOWN);
        if (stack_mem == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto stack_cap_res = pcb->cholder->insert_to_free(stack_mem);
        if (!stack_cap_res.has_value()) {
            delete stack_mem;
            propagate_return(stack_cap_res);
        }
        auto vma_res =
            pcb->tmm->add_vma(VMA::Type::STACK, VMA::Growth::GROW_DOWN,
                              VirArea(USER_STACK_BOTTOM, USER_STACK_TOP),
                              stack_mem, PageMan::RWX::RW);
        propagate(vma_res);
        loggers::TASK::INFO(
            "创建STACK VMA: pid=%lu area=[%p,%p) mem=%p memsz=%lu mem_off=%lu",
            pcb->pid, USER_STACK_BOTTOM.addr(), USER_STACK_TOP.addr(),
            stack_mem, static_cast<unsigned long>(MAX_INITIAL_STACK_SIZE), 0UL);

        auto tcb_cap_res = pcb->cholder->create<cap::TCBPayload>(tcb.get());
        propagate(tcb_cap_res);

        pcb->main_tcb_cap          = tcb_cap_res.value();
        startup_info.pcb_cap       = pcb->pcb_cap;
        startup_info.main_tcb_cap  = pcb->main_tcb_cap;
        startup_info.stack_mem_cap = stack_cap_res.value();
        auto stack_top_res =
            build_user_stack(*stack_mem, USER_STACK_TOP, startup_info,
                             spec.startup_blob.get(), spec.startup_blob_size);
        propagate(stack_top_res);
        loggers::TASK::DEBUG("栈内存分配: pid=%lu 栈内存地址=%p 已分配=%lu",
                             pcb->pid, stack_mem, stack_mem->allocated_size());
        reset_thread_runtime(tcb);
        build_user_contexts(tcb, pcb->entrypoint.addr(),
                            stack_top_res.value().addr());
        if (link_into_pcb) {
            pcb->threads.push_back(*tcb);
        }
        loggers::TASK::INFO("%s: pid=%lu entry=%p sp=%p", log_tag, pcb->pid,
                            pcb->entrypoint.addr(),
                            stack_top_res.value().addr());

        if (spec.startup_blob != nullptr) {
            delete[] spec.startup_blob.get();
            spec.startup_blob      = util::owner<char *>(nullptr);
            spec.startup_blob_size = 0;
        }
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::populate_task(
        util::nonnull<PCB *> pcb, TaskSpec spec, schd::ClassType schd_class,
        TCB *reuse_main_tcb) {
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (spec.tmm.get() == nullptr || spec.holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        pcb->tmm        = spec.tmm;
        pcb->cholder    = spec.holder;
        pcb->entrypoint = spec.entrypoint;

        if (pcb->pcb_cap == cap::null) {
            auto pcb_cap_res = insert_pcb_cap(pcb);
            propagate(pcb_cap_res);
            pcb->pcb_cap = pcb_cap_res.value();
        } else {
            auto pcb_cap_res = pcb->cholder->lookup(pcb->pcb_cap);
            propagate(pcb_cap_res);
        }

        task::StartupInfo startup_info{
            .heap_vaddr    = spec.heap_vaddr,
            .stack_vaddr   = USER_STACK_BOTTOM,
            .entrypoint    = spec.entrypoint,
            .pcb_cap       = pcb->pcb_cap,
            .main_tcb_cap  = cap::null,
            .heap_mem_cap  = spec.heap_mem_cap,
            .stack_mem_cap = cap::null,
        };

        if (reuse_main_tcb == nullptr) {
            auto con_res = construct_thread(pcb, pcb->entrypoint.addr(),
                                            USER_STACK_TOP.addr(), schd_class);
            propagate(con_res);
            auto tcb = con_res.value();
            auto tcb_guard =
                util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });
            auto setup_res = setup_user_main_thread(
                pcb, tcb, spec, startup_info, false, "构造用户主线程");
            propagate(setup_res);
            tcb_guard.release();
            return setup_res.value();
        }

        auto tcb        = util::nnullforce(reuse_main_tcb);
        tcb->task       = pcb;
        tcb->is_kernel  = false;
        tcb->boot_role  = BootThreadRole::NONE;
        tcb->schd_class = schd_class;
        return setup_user_main_thread(pcb, tcb, spec, startup_info, true,
                                      "复用主线程上下文");
    }

    Result<void> TaskManager::recycle_tcb(util::nonnull<TCB *> tcb) {
        loggers::TASK::DEBUG("回收线程 %d (PID: %d)", tcb->tid,
                             tcb->task != nullptr ? tcb->task->pid : -1);
        if (schd::Scheduler::initialized() &&
            schd::Scheduler::inst().current_tcb() == tcb.get())
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        if (tcb->basic_entity.state == ThreadState::READY &&
            tcb->schd_class != schd::ClassType::BOT)
        {
            auto dequeue_res = schd::Scheduler::inst().dequeue(tcb);
            propagate(dequeue_res);
        }
        if (tcb->basic_entity.state == ThreadState::WAITING &&
            wait::WaitReasonManager::initialized())
        {
            auto remove_res = wait::WaitReasonManager::inst().remove(tcb.get());
            propagate(remove_res);
        }

        PCB *pcb = tcb->task;
        if (pcb != nullptr) {
            pcb->threads.remove(*tcb);
        }

        if (tcb->kstack_phy.nonnull()) {
            GFP::put_page(tcb->kstack_phy - TCB::KSTACK_SIZE,
                          TCB::KSTACK_PAGES);
        }
        tcb->task          = nullptr;
        tcb->kstack_phy    = PhyAddr::null;
        tcb->kstack_bottom = nullptr;
        delete tcb.get();
        void_return();
    }

    Result<void> TaskManager::terminate_tcb(util::nonnull<TCB *> tcb) {
        return recycle_tcb(tcb);
    }

    Result<void> TaskManager::terminate_pcb(util::nonnull<PCB *> pcb) {
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        // terminate all threads in this process
        while (!pcb->threads.empty()) {
            TCB *tcb      = &pcb->threads.front();
            tid_t tid     = tcb->tid;
            auto term_res = terminate_tcb(util::nnullforce(tcb));
            if (!term_res.has_value()) {
                loggers::SUSTCORE::ERROR("终止线程 %d 失败! 错误码: %s", tid,
                                         to_cstring(term_res.error()));
                propagate_return(term_res);
            }
        }

        if (pcb->tmm.get() != nullptr) {
            PhyAddr pgd = pcb->tmm->pgd();
            delete pcb->tmm;
            GFP::put_page(pgd, 1);
        }
        if (pcb->cholder != nullptr) {
            auto &chman = cap::CHolderManager::inst();
            auto rm_res = chman.remove_holder(pcb->cholder->id());
            propagate(rm_res);
        }

        _pid_map.erase(pcb->pid);

        delete pcb.get();

        void_return();
    }

    void TaskManager::enqueue_recycle(PCB *pcb) {
        if (pcb == nullptr || !pcb->exiting || pcb->recycle_queued) {
            return;
        }
        pcb->recycle_queued = true;
        _recycle_pcbs.push_back(pcb);
    }

    void TaskManager::reap_recycled() {
        while (!_recycle_pcbs.empty()) {
            PCB *pcb = _recycle_pcbs.front();
            _recycle_pcbs.pop_front();
            if (pcb == nullptr) {
                continue;
            }
            loggers::TASK::INFO("回收退出进程: pid=%lu", pcb->pid);
            terminate_pcb(util::nnullforce(pcb));
        }
    }

    Result<util::nonnull<PCB *>> TaskManager::kernel_pcb() {
        if (_kernel_pcb == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return util::nnullforce(_kernel_pcb);
    }

    Result<util::nonnull<PCB *>> TaskManager::create_kernel_task() {
        if (_kernel_pcb != nullptr) {
            return util::nnullforce(_kernel_pcb);
        }

        util::nonnull<PCB *> pcb = alloc_pcb();
        auto pcb_guard           = delete_guard(util::owner(pcb.get()));
        auto init_res            = init_pcb(pcb);
        propagate(init_res);

        pcb->is_kernel = true;

        auto tmm_res =
            TaskMemoryManager::from_existing_pgd(env::inst().main_kernel_pgd());
        if (!tmm_res.has_value()) {
            loggers::SUSTCORE::FATAL("无法绑定主内核页表: %s",
                                     to_cstring(tmm_res.error()));
            propagate_return(tmm_res);
        }
        auto tmm_guard  = delete_guard(tmm_res.value());
        pcb->tmm        = tmm_res.value();
        auto create_res = cap::CHolderManager::inst().create_holder();
        propagate(create_res);
        pcb->cholder      = create_res.value();
        pcb->entrypoint   = VirAddr(nullptr);
        pcb->pcb_cap      = cap::null;
        pcb->main_tcb_cap = cap::null;

        _kernel_pcb        = pcb.get();
        _pid_map[pcb->pid] = pcb.get();
        pcb_guard.release();
        tmm_guard.release();
        return pcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::create_kernel_thread(
        void (*entry)(), schd::ClassType schd_class) {
        return create_kernel_thread(
            &kthread_noarg_entry, reinterpret_cast<void *>(entry), schd_class);
    }

    Result<util::nonnull<TCB *>> TaskManager::create_kernel_thread(
        KThreadEntry entry, void *arg, schd::ClassType schd_class) {
        if (entry == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (schd_class == schd::ClassType::BOT) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        auto pcb_res = kernel_pcb();
        propagate(pcb_res);
        auto tcb_res = construct_thread(
            pcb_res.value(), reinterpret_cast<void *>(&kthread_trampoline),
            nullptr, schd_class, true);
        propagate(tcb_res);
        util::nonnull<TCB *> tcb = tcb_res.value();
        tcb->kentry              = entry;
        tcb->karg                = arg;
        reset_thread_runtime(tcb);
        build_kernel_context(tcb, reinterpret_cast<void *>(&kthread_trampoline),
                             nullptr);
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::create_idle_thread() {
        return create_kernel_thread(&kthread_idle, schd::ClassType::IDLE);
    }

    Result<util::nonnull<TCB *>> TaskManager::create_kinit_thread() {
        auto tcb_res =
            create_kernel_thread(&kinit_runtime_entry, schd::ClassType::INIT);
        propagate(tcb_res);
        tcb_res.value()->boot_role = BootThreadRole::KINIT;
        return tcb_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::create_init_task(
        TaskSpec spec /* ... args*/) {
        constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::INIT;
        return create_loaded_user_task(spec, INIT_SCHED_CLASS, false);
    }

    Result<util::nonnull<PCB *>> TaskManager::create_task(
        TaskSpec spec, schd::ClassType schd_class /* ... args*/) {
        if (schd_class == schd::ClassType::INIT ||
            schd_class == schd::ClassType::IDLE ||
            schd_class == schd::ClassType::BOT)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return create_loaded_user_task(spec, schd_class, true);
    }

    Result<util::nonnull<PCB *>> TaskManager::create_loaded_user_task(
        TaskSpec spec, schd::ClassType schd_class, bool wakeup) {
        util::nonnull<PCB *> pcb = alloc_pcb();
        auto pcb_guard           = delete_guard(util::owner(pcb.get()));

        auto init_res = init_pcb(pcb);
        if (!init_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化PCB失败! 错误码: %s",
                                     to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        auto main_thread_res = populate_task(pcb, spec, schd_class);
        if (!main_thread_res.has_value()) {
            loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                     to_cstring(main_thread_res.error()));
            propagate_return(main_thread_res);
        }
        if (schd_class == schd::ClassType::INIT) {
            main_thread_res.value()->boot_role = BootThreadRole::INIT_USER;
        }

        if (wakeup) {
            TCB *main_tcb = main_thread_res.value();
            if (!schd::Scheduler::inst().wakeup_new(main_tcb)) {
                loggers::SUSTCORE::ERROR("唤醒新进程失败");
                unexpect_return(ErrCode::CREATION_FAILED);
            }
        }

        _pid_map[pcb->pid] = pcb.get();
        pcb_guard.release();  // 进程已成功构造并加入调度队列
        return pcb;
    }

    Result<CapIdx> TaskManager::preload(CapIdx image_cap, TaskSpec &spec,
                                        LoadPrm &prm) {
        // 构造cholder
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            loggers::SUSTCORE::ERROR("创建CHolder失败! 错误码: %s",
                                     to_cstring(create_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto holder = create_res.value();

        auto preload_res = preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            assert(rm_res.has_value());
            propagate_return(preload_res);
        }
        return preload_res;
    }

    Result<CapIdx> TaskManager::preload_into(CapIdx image_cap,
                                             cap::CHolder *holder,
                                             TaskSpec &spec, LoadPrm &prm) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        // 申请一个页表以构造task memory manager
        auto gfp_res = GFP::get_free_page(1);
        if (!gfp_res.has_value()) {
            loggers::SUSTCORE::ERROR("无法为程序页表分配物理页");
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto tmm = util::owner(new TaskMemoryManager(gfp_res.value()));

        auto cap_res = holder->lookup(image_cap);
        propagate(cap_res);
        if (cap_res.value()->payload()->type_id() != PayloadType::VFILE) {
            unexpect_return(ErrCode::TYPE_NOT_MATCHED);
        }
        if (!cap_res.value()->imply(perm::vfile::EXEC)) {
            unexpect_return(ErrCode::INSUFFICIENT_PERMISSIONS);
        }

        // 设置spec参数
        spec.holder = holder;
        spec.tmm    = tmm;

        // 设置prm参数
        prm.src_path       = "<cap>";
        prm.image_file_cap = image_cap;
        return image_cap;
    }

    Result<void> TaskManager::validate_bootstrap_blob(
        const void *startup_blob, size_t startup_blob_size) {
        if (startup_blob_size == 0) {
            void_return();
        }
        if (startup_blob == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        bool valid  = true;
        bool walked = bootstrap_foreach_record(
            startup_blob, startup_blob_size,
            [&](const BootstrapRecordView &view) {
                if (!valid) {
                    return;
                }
                if (view.header->type == BOOTSTRAP_TYPE_FILECAPEXPLAIN ||
                    view.header->type == BOOTSTRAP_TYPE_DIRCAPEXPLAIN)
                {
                    BootstrapCapPathView cap_path{};
                    valid = bootstrap_parse_cap_path(view, cap_path);
                    if (!valid || !valid_bootstrap_cap_path(cap_path)) {
                        return;
                    }
                    return;
                }

                if (view.header->type == 0) {
                    valid = false;
                }
            });
        if (!walked || !valid) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        void_return();
    }

    Result<void> TaskManager::build_bootstrap_blob(
        const void *startup_blob, size_t startup_blob_size,
        const std::vector<BootstrapCapPathView> &dir_records,
        const std::vector<BootstrapCapPathView> &file_records, TaskSpec &spec) {
        auto validate_res =
            validate_bootstrap_blob(startup_blob, startup_blob_size);
        propagate(validate_res);

        size_t total_size = startup_blob_size;
        for (const auto &record : dir_records) {
            if (!valid_bootstrap_cap_path(record)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            total_size += bootstrap_cap_path_record_size(record);
        }
        for (const auto &record : file_records) {
            if (!valid_bootstrap_cap_path(record)) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
            total_size += bootstrap_cap_path_record_size(record);
        }

        if (total_size == 0) {
            if (spec.startup_blob != nullptr) {
                delete[] spec.startup_blob.get();
                spec.startup_blob      = util::owner<char *>(nullptr);
                spec.startup_blob_size = 0;
            }
            void_return();
        }

        auto merged = util::owner(new char[total_size]);
        if (merged.get() == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }

        size_t offset = 0;
        std::vector<size_t> record_offsets{};
        if (startup_blob_size != 0) {
            bool copied = true;
            bool walked = bootstrap_foreach_record(
                startup_blob, startup_blob_size,
                [&](const BootstrapRecordView &view) {
                    if (!copied) {
                        return;
                    }
                    record_offsets.push_back(offset);
                    auto append_res = append_bootstrap_record(
                        merged.get(), total_size, offset, view.header->type,
                        view.data, view.data_size);
                    copied = append_res.has_value();
                });
            if (!walked || !copied) {
                unexpect_return(ErrCode::INVALID_PARAM);
            }
        }

        for (const auto &record : dir_records) {
            record_offsets.push_back(offset);
            auto append_res = append_bootstrap_cap_path_record(
                merged.get(), total_size, offset, BOOTSTRAP_TYPE_DIRCAPEXPLAIN,
                record);
            propagate(append_res);
        }
        for (const auto &record : file_records) {
            record_offsets.push_back(offset);
            auto append_res = append_bootstrap_cap_path_record(
                merged.get(), total_size, offset, BOOTSTRAP_TYPE_FILECAPEXPLAIN,
                record);
            propagate(append_res);
        }

        finalize_bootstrap_record_next(merged.get(), record_offsets);
        if (spec.startup_blob != nullptr) {
            delete[] spec.startup_blob.get();
        }
        spec.startup_blob      = std::move(merged);
        spec.startup_blob_size = total_size;
        void_return();
    }

    Result<void> TaskManager::load_task_spec(CapIdx image_cap,
                                             cap::CHolder *holder,
                                             const void *startup_blob,
                                             size_t startup_blob_size,
                                             TaskSpec &spec, LoadPrm &prm) {
        std::vector<BootstrapCapPathView> dir_records{};
        std::vector<BootstrapCapPathView> file_records{};
        auto bootstrap_res = build_bootstrap_blob(
            startup_blob, startup_blob_size, dir_records, file_records, spec);
        propagate(bootstrap_res);

        auto preload_res = holder == nullptr
                               ? preload(image_cap, spec, prm)
                               : preload_into(image_cap, holder, spec, prm);
        if (!preload_res.has_value()) {
            loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        auto load_res = loader::elf::load(spec, prm);
        if (!load_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载ELF程序失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        void_return();
    }

    Result<util::nonnull<PCB *>> TaskManager::load_task_image(
        CapIdx image_cap, cap::CHolder *holder, schd::ClassType schd_class,
        bool wakeup, const void *startup_blob, size_t startup_blob_size) {
        TaskSpec spec = empty_task_spec();
        LoadPrm load_prm{};
        auto load_spec_res = load_task_spec(image_cap, holder, startup_blob,
                                            startup_blob_size, spec, load_prm);
        if (!load_spec_res.has_value()) {
            destroy_unowned_task_memory(spec);
            propagate_return(load_spec_res);
        }

        bool spec_owned = true;
        auto spec_guard = util::Guard([&]() {
            if (spec_owned) {
                destroy_unowned_task_memory(spec);
            }
        });

        auto task_res = create_loaded_user_task(spec, schd_class, wakeup);
        propagate(task_res);
        spec_owned = false;
        return task_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::load_elf(
        CapIdx image_cap, schd::ClassType schd_class) {
        return load_task_image(image_cap, nullptr, schd_class, true);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_elf_into(
        CapIdx image_cap, cap::CHolder *holder, schd::ClassType schd_class,
        const void *startup_blob, size_t startup_blob_size) {
        return load_task_image(image_cap, holder, schd_class, true,
                               startup_blob, startup_blob_size);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_init(const char *path) {
        constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::INIT;
        if (path == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto *holder      = create_res.value();
        auto holder_guard = util::Guard([holder]() {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            assert(rm_res.has_value());
        });
        auto image_res    = VFS::inst().open(path, *holder);
        propagate(image_res);
        auto root_res =
            VFS::inst().open_dir("/", *holder,
                                 perm::vdir::READ | perm::vdir::WRITE |
                                     perm::vdir::EXEC | perm::basic::CLONE);
        propagate(root_res);
        std::vector<BootstrapCapPathView> dir_records{};
        std::vector<BootstrapCapPathView> file_records{};
        dir_records.push_back(BootstrapCapPathView{
            .cap  = root_res.value(),
            .path = "/",
        });

        TaskSpec spec = empty_task_spec();
        LoadPrm load_prm{};
        auto bootstrap_res =
            build_bootstrap_blob(nullptr, 0, dir_records, file_records, spec);
        propagate(bootstrap_res);
        auto load_spec_res =
            load_task_spec(image_res.value(), holder, spec.startup_blob.get(),
                           spec.startup_blob_size, spec, load_prm);
        if (!load_spec_res.has_value()) {
            destroy_unowned_task_memory(spec);
            propagate_return(load_spec_res);
        }
        bool spec_owned = true;
        auto spec_guard = util::Guard([&]() {
            if (spec_owned) {
                destroy_unowned_task_memory(spec);
            }
        });
        auto load_res = create_loaded_user_task(spec, INIT_SCHED_CLASS, false);
        propagate(load_res);
        spec_owned = false;
        holder_guard.release();
        return load_res.value();
    }

    Result<size_t> TaskManager::lookup_holder_id(pid_t pid) {
        return _pid_map.at_nt(pid)
            .transform_error(always(ErrCode::OUT_OF_BOUNDARY))
            .and_then([](auto *pcb_ptr) -> Result<size_t> {
                PCB *pcb = pcb_ptr == nullptr ? nullptr : *pcb_ptr;
                if (pcb == nullptr || pcb->cholder == nullptr) {
                    unexpect_return(ErrCode::INVALID_PARAM);
                }
                return pcb->cholder->id();
            });
    }

    Result<util::nonnull<TCB *>> TaskManager::populate_forked_task(
        util::nonnull<PCB *> child_pcb, PCB *parent_pcb, TCB *parent_tcb,
        Context *parent_ctx, CapIdx ret_slot) {
        if (parent_pcb == nullptr || parent_tcb == nullptr ||
            parent_ctx == nullptr || parent_pcb->tmm == nullptr ||
            parent_pcb->cholder == nullptr)
        {
            unexpect_return(ErrCode::NULLPTR);
        }

        // 1) 构造子进程专属资源, 以父进程地址空间作为初始填充来源.
        auto holder_res = cap::CHolderManager::inst().create_holder();
        propagate(holder_res);
        cap::CHolder *child_holder = holder_res.value();
        auto holder_guard          = util::Guard([child_holder]() {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(child_holder->id());
            assert(rm_res.has_value());
        });

        auto pgd_res = GFP::get_free_page(1);
        propagate(pgd_res);
        auto child_tmm = util::owner(new TaskMemoryManager(pgd_res.value()));
        auto tmm_guard = util::Guard([&child_tmm]() {
            if (child_tmm.get() == nullptr) {
                return;
            }
            PhyAddr pgd = child_tmm->pgd();
            delete child_tmm;
            GFP::put_page(pgd, 1);
        });

        auto clone_mem_res = parent_pcb->tmm->clone_to_cow(*child_tmm);
        propagate(clone_mem_res);

        // 2) 克隆 capability 空间, 并把 COW 后的内存 payload 绑定到子 TMM.
        ErrCode clone_caps_err = ErrCode::SUCCESS;
        parent_pcb->cholder->space().foreach (
            [&](CapIdx idx, cap::Capability *parent_cap) {
                if (clone_caps_err != ErrCode::SUCCESS) {
                    return;
                }
                cap::Payload *payload = parent_cap->payload();
                auto *memory = parent_cap->payload_as<cap::MemoryPayload>();
                if (memory != nullptr) {
                    auto child_memory_res =
                        parent_pcb->tmm->cloned_memory_for(memory, *child_tmm);
                    if (child_memory_res.has_value()) {
                        payload = child_memory_res.value();
                    } else {
                        payload = memory->clone_payload();
                    }
                }
                auto insert_res =
                    child_holder->insert(idx, payload, parent_cap->perm());
                if (!insert_res.has_value()) {
                    clone_caps_err = insert_res.error();
                }
            });
        if (clone_caps_err != ErrCode::SUCCESS) {
            unexpect_return(clone_caps_err);
        }

        // 3) 将已构造资源填入子 PCB, 再按 fork 返回语义构造主线程.
        child_pcb->tmm          = child_tmm;
        child_pcb->cholder      = child_holder;
        child_pcb->entrypoint   = parent_pcb->entrypoint;
        child_pcb->pcb_cap      = ret_slot;
        child_pcb->main_tcb_cap = parent_pcb->main_tcb_cap;

        util::nonnull<TCB *> child_tcb = alloc_tcb();
        auto tcb_guard =
            util::Guard([this, child_tcb]() { (void)recycle_tcb(child_tcb); });
        auto init_tcb_res = init_tcb(child_tcb, child_pcb);
        propagate(init_tcb_res);

        reset_thread_runtime(child_tcb);
        child_tcb->reset_kstack();
        auto *child_user_ctx = child_tcb->push<Context>();
        *child_user_ctx      = *parent_ctx;
        child_user_ctx->kstack_top() =
            reinterpret_cast<umb_t>(child_tcb->kstack_top());
        write_ret(*child_user_ctx,
                  syscall::RetPack{
                      .processed = true,
                      .ret0      = 0,
                      .ret1      = static_cast<b64>(ErrCode::SUCCESS),
                  });
        init_kernel_context(child_tcb->kernel_context_ptr(),
                            reinterpret_cast<void *>(&new_utask_trampoline),
                            child_user_ctx, child_tcb->kstack_top(), false,
                            true);
        child_tcb->boot_role  = parent_tcb->schd_class == schd::ClassType::INIT
                                    ? BootThreadRole::NONE
                                    : parent_tcb->boot_role;
        child_tcb->schd_class = parent_tcb->schd_class == schd::ClassType::INIT
                                    ? schd::ClassType::FCFS
                                    : parent_tcb->schd_class;
        child_pcb->threads.push_back(*child_tcb);
        tcb_guard.release();

        // 4) 在父子 capability 空间的同一槽位放入子 PCB capability.
        auto *pcb_payload = new cap::PCBPayload(child_pcb.get());
        if (pcb_payload == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto insert_parent_res =
            parent_pcb->cholder->insert(ret_slot, pcb_payload, perm::allperm());
        if (!insert_parent_res.has_value()) {
            delete pcb_payload;
            propagate_return(insert_parent_res);
        }
        auto insert_child_res =
            child_holder->insert(ret_slot, pcb_payload, perm::allperm());
        if (!insert_child_res.has_value()) {
            auto remove_res = parent_pcb->cholder->remove(ret_slot);
            assert(remove_res.has_value());
            propagate_return(insert_child_res);
        }

        tmm_guard.release();
        holder_guard.release();
        return child_tcb;
    }

    Result<ForkResult> TaskManager::fork_current(CapIdx ret_slot) {
        auto *parent_tcb = schd::Scheduler::inst().current_tcb();
        auto *parent_ctx = env::inst().trap_context();
        if (parent_tcb == nullptr || parent_tcb->task == nullptr ||
            parent_ctx == nullptr)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        PCB *parent_pcb = parent_tcb->task;
        if (parent_pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (parent_pcb->tmm == nullptr || parent_pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        util::nonnull<PCB *> child_pcb = alloc_pcb();
        auto pcb_guard = delete_guard(util::owner(child_pcb.get()));
        auto init_res  = init_pcb(child_pcb);
        propagate(init_res);
        auto fill_res = populate_forked_task(child_pcb, parent_pcb, parent_tcb,
                                             parent_ctx, ret_slot);
        propagate(fill_res);
        TCB *child_tcb = fill_res.value();

        _pid_map[child_pcb->pid] = child_pcb.get();
        if (!schd::Scheduler::inst().wakeup_new(child_tcb)) {
            _pid_map.erase(child_pcb->pid);
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        ForkResult result{ret_slot, child_pcb->pid};
        pcb_guard.release();
        return result;
    }

    Result<CapIdx> TaskManager::create_thread_current(VirAddr entry,
                                                      VirAddr stack_addr,
                                                      size_t stack_size) {
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        if (current_tcb == nullptr || current_tcb->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        PCB *pcb = current_tcb->task;
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (pcb->tmm == nullptr || pcb->cholder == nullptr ||
            !entry.nonnull() || !stack_addr.nonnull() || stack_size == 0)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (current_tcb->schd_class == schd::ClassType::IDLE) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }

        addr_t stack_base = stack_addr.arith();
        if (stack_base > MAX_ADDR - stack_size) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        VirAddr stack_top(stack_base + stack_size);
        VirArea stack_area(stack_addr, stack_top);
        auto range_res = pcb->tmm->locate_range(stack_area);
        propagate(range_res);

        auto con_res =
            construct_thread(util::nnullforce(pcb), entry.addr(),
                             stack_top.addr(), current_tcb->schd_class);
        propagate(con_res);
        util::nonnull<TCB *> tcb = con_res.value();
        auto tcb_guard = util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });

        auto cap_res = pcb->cholder->create<cap::TCBPayload>(tcb.get());
        propagate(cap_res);
        auto cap_guard = remove_guard(pcb->cholder, cap_res.value());

        if (!schd::Scheduler::inst().wakeup_new(tcb.get())) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        cap_guard.release();
        tcb_guard.release();
        return cap_res.value();
    }

    Result<void> TaskManager::exec_current(CapIdx image_cap,
                                           const CapIdx *reserved_caps,
                                           size_t reserved_count) {
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        if (current_tcb == nullptr || current_tcb->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return exec_pcb(util::nnullforce(current_tcb->task), image_cap,
                        reserved_caps, reserved_count, nullptr, 0);
    }

    Result<void> TaskManager::exec_pcb(util::nonnull<PCB *> target,
                                       CapIdx image_cap,
                                       const CapIdx *reserved_caps,
                                       size_t reserved_count,
                                       const void *startup_blob,
                                       size_t startup_blob_size) {
        PCB *pcb = target.get();
        if (pcb->is_kernel) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        if (pcb->tmm == nullptr || pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        if (startup_blob_size != 0 && startup_blob == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        bool target_current =
            current_tcb != nullptr && current_tcb->task == pcb;
        schd::ClassType schd_class =
            exec_sched_class(pcb, current_tcb, target_current);
        TCB *reuse_tcb = target_current ? current_tcb : nullptr;

        // 1) 在破坏旧进程状态前完成 capability 校验与新镜像加载.
        auto reserved_res = verify_reserved_caps(pcb->cholder, pcb->pcb_cap,
                                                 reserved_caps, reserved_count);
        propagate(reserved_res);

        TaskSpec spec = empty_task_spec();
        LoadPrm load_prm{};
        auto load_spec_res =
            load_task_spec(image_cap, pcb->cholder, startup_blob,
                           startup_blob_size, spec, load_prm);
        propagate(load_spec_res);
        bool spec_owned = true;
        auto spec_guard = util::Guard([&]() {
            if (spec_owned) {
                destroy_unowned_task_memory(spec);
            }
        });
        // TODO: 接下来的操作会对当前进程的内存空间和能力空间进行大幅修改,
        // 应妥善处理执行失败的情况

        // 2) 清空当前 PCB 中会被新镜像替换的主体状态, 只保留指定能力.
        auto prune_res = remove_unreserved_caps(pcb->cholder, pcb->pcb_cap,
                                                reserved_caps, reserved_count);
        propagate(prune_res);

        TaskMemoryManager *old_tmm = pcb->tmm;
        while (!pcb->threads.empty()) {
            TCB *tcb = &pcb->threads.front();
            pcb->threads.pop_front();
            if (tcb == reuse_tcb) {
                continue;
            }
            if (tcb->basic_entity.state == ThreadState::READY) {
                auto dequeue_res =
                    schd::Scheduler::inst().dequeue(util::nnullforce(tcb));
                propagate(dequeue_res);
            }
            auto recycle_res = recycle_tcb(util::nnullforce(tcb));
            propagate(recycle_res);
        }
        pcb->tmm          = util::owner<TaskMemoryManager *>(nullptr);
        pcb->entrypoint   = VirAddr(static_cast<addr_t>(0));
        pcb->main_tcb_cap = cap::null;

        // 3) 将新 TaskSpec 填入已清空的 PCB, 完成 exec 后主线程构造.
        auto populate_res =
            populate_task(util::nnullforce(pcb), spec, schd_class, reuse_tcb);
        propagate(populate_res);

        // 4) 当前进程 exec 直接恢复运行; 非当前目标需要唤醒新主线程.
        if (target_current) {
            current_tcb->basic_entity.state = ThreadState::RUNNING;
            current_tcb->basic_entity.flags = 0;
            current_tcb->rr_entity          = {};
        } else if (!schd::Scheduler::inst().wakeup_new(populate_res.value())) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        spec_owned = false;
        spec_guard.release();

        if (target_current) {
            schd::switch_pgd(pcb->tmm);
        }

        TaskSpec old_spec{
            .tmm        = util::owner(old_tmm),
            .holder     = nullptr,
            .entrypoint = VirAddr(static_cast<addr_t>(0)),
        };
        destroy_unowned_task_memory(old_spec);

        loggers::SUSTCORE::DEBUG("execve成功: image_cap=%p pid=%d", image_cap,
                                 pcb->pid);
        void_return();
    }

    namespace kop {
        KOP<PCB> pcb;
        KOP<TCB> tcb;
    }  // namespace kop

    void init_kop() {
        new (&kop::pcb) KOP<PCB>();
        new (&kop::tcb) KOP<TCB>();
    }

    void *PCB::operator new(size_t size) {
        assert(size == sizeof(PCB));
        return kop::pcb.alloc();
    }

    void PCB::operator delete(void *ptr) {
        kop::pcb.free(static_cast<PCB *>(ptr));
    }

    void *TCB::operator new(size_t size) {
        assert(size == sizeof(TCB));
        return kop::tcb.alloc();
    }

    void TCB::operator delete(void *ptr) {
        kop::tcb.free(static_cast<TCB *>(ptr));
    }
}  // namespace task
