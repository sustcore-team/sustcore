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
#include <env.h>
#include <exe/elfloader.h>
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
#include <vfs/vfs.h>

namespace task {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    static TaskManager inst_task_manager;

    void TaskManager::init() {
        // call the constructor explicitly to ensure the instance is initialized
        // before use
        new (&inst_task_manager) TaskManager();
    }

    TaskManager &TaskManager::inst() {
        return inst_task_manager;
    }

    static Result<CapIdx> insert_pcb_cap(PCB *pcb) {
        if (pcb == nullptr || pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *payload = new cap::PCBPayload(pcb);
        if (payload == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto insert_res = pcb->cholder->internal_insert_to_free(payload);
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
    static bool capidx_in_list(CapIdx idx, const CapIdx *list, size_t count) {
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
    static Result<void> verify_reserved_caps(cap::CHolder *holder,
                                             CapIdx pcb_cap,
                                             const CapIdx *reserved_caps,
                                             size_t reserved_count) {
        if (holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto pcb_cap_res = holder->internal_lookup(pcb_cap);
        propagate(pcb_cap_res);
        for (size_t i = 0; i < reserved_count; ++i) {
            CapIdx idx   = reserved_caps[i];
            auto cap_res = holder->internal_lookup(idx);
            propagate(cap_res);
        }
        void_return();
    }

    /**
     * @brief 移除 holder 中除了 pcb_cap 和 reserved_caps
     * 列表中索引以外的所有能力, 用于 exec 替换镜像时清理不需要保留的能力槽位.
     *
     * @param holder 能力持有者, 用于执行能力移除操作.
     * @param pcb_cap PCB 的能力索引, 需要保留.
     * @param reserved_caps 需要保留的能力索引列表, 每个索引都需要保留.
     * @param reserved_count reserved_caps 中的元素数量.
     * @return Result<void> 成功返回 SUCCESS, 失败返回相应错误码.
     */
    static Result<void> remove_unreserved_caps(cap::CHolder *holder,
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
            auto remove_res = holder->internal_remove(idx);
            if (!remove_res.has_value()) {
                err = remove_res.error();
            }
        });
        if (err != ErrCode::SUCCESS) {
            unexpect_return(err);
        }
        void_return();
    }

    Result<void> TaskManager::init_tcb(
        util::nonnull<TCB *> tcb, util::nonnull<PCB *> task /* ... args*/) {
        // 初始化 TCB 基本信息
        tcb->tid            = alloc_tid();
        tcb->task           = task;
        tcb->list_head      = {};
        tcb->wait_reason    = 0;
        tcb->wait_predicate = {};
        tcb->wait_head      = {};
        tcb->coroutines     = {};

        // ask for a kstack for this thread
        Result<PhyAddr> gfp_res = GFP::get_free_page(TCB::KSTACK_PAGES);
        propagate(gfp_res);

        // calculate the top of the kernel stack
        tcb->kstack_phy = gfp_res.value() + TCB::KSTACK_PAGES * PAGESIZE;
        tcb->kstack_top = convert<KpaAddr>(tcb->kstack_phy).addr();

        // initialization the kstack and context
        void_return();
    }

    Result<void> TaskManager::init_ctx(util::nonnull<TCB *> tcb,
                                       void *entrypoint, void *stack_top) {
        *tcb->context() = {};
        tcb->context()->setup_regs(false);
        tcb->context()->pc() = reinterpret_cast<umb_t>(entrypoint);
        tcb->context()->sp() = reinterpret_cast<umb_t>(stack_top);
        tcb->basic_entity    = {};
        tcb->rr_entity       = {};
        tcb->coroutines      = {};

        void_return();
    }

    Result<void> TaskManager::init_pcb(util::nonnull<PCB *> pcb,
                                       TaskSpec spec /* ... args*/) {
        pcb->pid            = alloc_pid();
        pcb->exit_code      = 0;
        pcb->threads        = {};
        pcb->exiting        = false;
        pcb->recycle_queued = false;

        // initialize the pcb according to the specification
        pcb->tmm          = spec.tmm;
        pcb->cholder      = spec.holder;
        pcb->entrypoint   = spec.entrypoint;
        pcb->pcb_cap      = cap::null;
        pcb->main_tcb_cap = cap::null;
        void_return();
    }

    Result<util::nonnull<TCB *>> TaskManager::construct_thread(
        util::nonnull<PCB *> pcb, void *entrypoint, void *stack_top,
        schd::ClassType schd_class) {
        util::nonnull<TCB *> tcb = alloc_tcb();
        auto tcb_guard = util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });
        auto init_res  = init_tcb(tcb, pcb);
        if (!init_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化TCB失败! 错误码: %s",
                                     to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        auto ctx_res = init_ctx(tcb, entrypoint, stack_top);
        if (!ctx_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化线程上下文失败! 错误码: %s",
                                     to_cstring(ctx_res.error()));
            propagate_return(ctx_res);
        }

        tcb->schd_class = schd_class;

        // 将线程加入进程的线程列表
        pcb->threads.push_back(*tcb);
        tcb_guard.release();  // 线程已成功构造, 释放TCB的自动释放机制
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::construct_main_thread(
        util::nonnull<PCB *> pcb, schd::ClassType schd_class,
        task::StartupInfo startup_info) {
        // 为主线程分配初始栈空间, 并将其加入Task Memory的VMA中
        // 此处无需通过GFP分配物理页, 由缺页中断自动处理即可
        auto *stack_mem = new cap::MemoryPayload(MAX_INITIAL_STACK_SIZE, false,
                                                 false, VMA::Growth::GROW_DOWN);
        auto stack_cap_res = pcb->cholder->internal_insert_to_free(stack_mem);
        if (!stack_cap_res.has_value()) {
            delete stack_mem;
            propagate_return(stack_cap_res);
        }
        auto vma_res =
            pcb->tmm->add_vma(VMA::Type::STACK, VMA::Growth::GROW_DOWN,
                              VirArea(USER_STACK_BOTTOM, USER_STACK_TOP),
                              stack_mem, PageMan::RWX::RW);
        propagate(vma_res);

        auto con_res = construct_thread(pcb, pcb->entrypoint.addr(),
                                        USER_STACK_TOP.addr(), schd_class);
        propagate(con_res);
        util::nonnull<TCB *> tcb = con_res.value();
        auto tcb_guard = util::Guard([this, tcb]() { (void)recycle_tcb(tcb); });

        auto tcb_cap_slot_res = pcb->cholder->internal_lookup_freeslot();
        propagate(tcb_cap_slot_res);
        auto tcb_cap_res = pcb->cholder->internal_create<cap::TCBPayload>(
            tcb_cap_slot_res.value(), tcb.get());
        propagate(tcb_cap_res);

        pcb->main_tcb_cap          = tcb_cap_slot_res.value();
        startup_info.pcb_cap       = pcb->pcb_cap;
        startup_info.main_tcb_cap  = pcb->main_tcb_cap;
        startup_info.stack_mem_cap = stack_cap_res.value();
        tcb->context()->write_startup(startup_info);

        tcb_guard.release();
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::populate_task(
        util::nonnull<PCB *> pcb, TaskSpec spec, schd::ClassType schd_class,
        TCB *reuse_main_tcb) {
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
            auto pcb_cap_res = pcb->cholder->internal_lookup(pcb->pcb_cap);
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
            return construct_main_thread(pcb, schd_class, startup_info);
        }

        auto *stack_mem = new cap::MemoryPayload(MAX_INITIAL_STACK_SIZE, false,
                                                 false, VMA::Growth::GROW_DOWN);
        auto stack_cap_res = pcb->cholder->internal_insert_to_free(stack_mem);
        if (!stack_cap_res.has_value()) {
            delete stack_mem;
            propagate_return(stack_cap_res);
        }
        auto vma_res =
            pcb->tmm->add_vma(VMA::Type::STACK, VMA::Growth::GROW_DOWN,
                              VirArea(USER_STACK_BOTTOM, USER_STACK_TOP),
                              stack_mem, PageMan::RWX::RW);
        propagate(vma_res);

        auto tcb = util::nnullforce(reuse_main_tcb);
        auto ctx_res =
            init_ctx(tcb, pcb->entrypoint.addr(), USER_STACK_TOP.addr());
        propagate(ctx_res);
        tcb->task       = pcb;
        tcb->schd_class = schd_class;

        auto tcb_cap_slot_res = pcb->cholder->internal_lookup_freeslot();
        propagate(tcb_cap_slot_res);
        auto tcb_cap_res = pcb->cholder->internal_create<cap::TCBPayload>(
            tcb_cap_slot_res.value(), tcb.get());
        propagate(tcb_cap_res);

        pcb->main_tcb_cap          = tcb_cap_slot_res.value();
        startup_info.main_tcb_cap  = pcb->main_tcb_cap;
        startup_info.stack_mem_cap = stack_cap_res.value();
        tcb->context()->write_startup(startup_info);
        pcb->threads.push_back(*tcb);
        return tcb;
    }

    Result<void> TaskManager::recycle_tcb(util::nonnull<TCB *> tcb) {
        loggers::TASK::INFO("回收线程 %d (PID: %d)", tcb->tid,
                            tcb->task != nullptr ? tcb->task->pid : -1);
        PCB *pcb = tcb->task;
        if (pcb != nullptr) {
            pcb->threads.remove(*tcb);
        }

        if (tcb->kstack_phy.nonnull()) {
            GFP::put_page(tcb->kstack_phy - TCB::KSTACK_SIZE,
                          TCB::KSTACK_PAGES);
        }
        tcb->task       = nullptr;
        tcb->kstack_phy = PhyAddr::null;
        tcb->kstack_top = nullptr;
        delete tcb.get();
        void_return();
    }

    Result<void> TaskManager::terminate_tcb(util::nonnull<TCB *> tcb) {
        return recycle_tcb(tcb);
    }

    Result<void> TaskManager::terminate_pcb(util::nonnull<PCB *> pcb) {
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

        // free the tmm
        PhyAddr pgd = pcb->tmm->pgd();
        delete pcb->tmm;
        GFP::put_page(pgd, 1);
        // ask cholder manager to remove the cholder
        auto &chman = cap::CHolderManager::inst();
        auto rm_res = chman.remove_holder(pcb->cholder->id());
        propagate(rm_res);

        _pid_map.remove(pcb->pid);

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
            terminate_pcb(util::nnullforce(pcb));
        }
    }

    Result<util::nonnull<PCB *>> TaskManager::create_init_task(
        TaskSpec spec /* ... args*/) {
        constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::IDLE;
        util::nonnull<PCB *> pcb                   = alloc_pcb();
        auto pcb_guard = util::Guard([pcb]() { delete pcb.get(); });

        auto init_res = init_pcb(pcb, spec);
        if (!init_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化PCB失败! 错误码: %s",
                                     to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        auto main_thread_res = populate_task(pcb, spec, INIT_SCHED_CLASS);
        if (!main_thread_res.has_value()) {
            loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                     to_cstring(main_thread_res.error()));
            propagate_return(main_thread_res);
        }

        _pid_map.put(pcb->pid, pcb);
        pcb_guard.release();  // 进程已成功构造, 释放PCB的自动释放机制
        return pcb;
    }

    Result<util::nonnull<PCB *>> TaskManager::create_task(
        TaskSpec spec, schd::ClassType schd_class /* ... args*/) {
        util::nonnull<PCB *> pcb = alloc_pcb();
        auto pcb_guard           = util::Guard([pcb]() { delete pcb.get(); });

        auto init_res = init_pcb(pcb, spec);
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

        TCB *main_tcb = main_thread_res.value();
        if (!schd::Scheduler::inst().wakeup_new(main_tcb)) {
            loggers::SUSTCORE::ERROR("唤醒新进程失败");
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        _pid_map.put(pcb->pid, pcb);
        pcb_guard.release();  // 进程已成功构造并加入调度队列
        return pcb;
    }

    Result<CapIdx> TaskManager::preload(const char *path, TaskSpec &spec,
                                        LoadPrm &prm) {
        // 构造cholder
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            loggers::SUSTCORE::ERROR("创建CHolder失败! 错误码: %s",
                                     to_cstring(create_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto holder = create_res.value();

        auto preload_res = preload_into(path, holder, spec, prm);
        if (!preload_res.has_value()) {
            auto rm_res =
                cap::CHolderManager::inst().remove_holder(holder->id());
            assert(rm_res.has_value());
            propagate_return(preload_res);
        }
        return preload_res;
    }

    Result<CapIdx> TaskManager::preload_into(const char *path,
                                             cap::CHolder *holder,
                                             TaskSpec &spec, LoadPrm &prm) {
        if (path == nullptr || holder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        // 申请一个页表以构造task memory manager
        auto gfp_res = GFP::get_free_page(1);
        if (!gfp_res.has_value()) {
            loggers::SUSTCORE::ERROR("无法为程序页表分配物理页");
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto tmm = util::owner(new TaskMemoryManager(gfp_res.value()));

        // 打开文件
        auto open_res = VFS::inst().open(path);
        propagate(open_res);
        VFile *file = open_res.value();

        // 加载到CHolder中
        auto insert_res = holder->internal_insert_to_free(file);
        if (!insert_res.has_value()) {
            file->destruct();
            propagate_return(insert_res);
        }

        // 设置spec参数
        spec.holder = holder;
        spec.tmm    = tmm;

        // 设置prm参数
        prm.src_path       = path;
        prm.image_file_cap = insert_res.value();
        return insert_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::load_elf(
        const char *path, schd::ClassType schd_class) {
        TaskSpec spec{util::owner<TaskMemoryManager *>(nullptr), nullptr,
                      VirAddr(static_cast<addr_t>(0))};
        LoadPrm load_prm{};
        auto preload_res = preload(path, spec, load_prm);
        if (!preload_res.has_value()) {
            loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        // 开始加载程序
        auto load_res = loader::elf::load(spec, load_prm);
        if (!load_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载ELF程序失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        return create_task(spec, schd_class);
    }

    Result<util::nonnull<PCB *>> TaskManager::load_elf_into(
        const char *path, cap::CHolder *holder, schd::ClassType schd_class) {
        TaskSpec spec{util::owner<TaskMemoryManager *>(nullptr), nullptr,
                      VirAddr(static_cast<addr_t>(0))};
        LoadPrm load_prm{};
        auto preload_res = preload_into(path, holder, spec, load_prm);
        if (!preload_res.has_value()) {
            loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }
        bool spec_owned = true;
        auto spec_guard = util::Guard([&]() {
            if (!spec_owned || spec.tmm.get() == nullptr) {
                return;
            }
            PhyAddr pgd = spec.tmm->pgd();
            delete spec.tmm;
            GFP::put_page(pgd, 1);
        });

        auto load_res = loader::elf::load(spec, load_prm);
        if (!load_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载ELF程序失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        auto task_res = create_task(spec, schd_class);
        propagate(task_res);
        spec_owned = false;
        return task_res.value();
    }

    Result<util::nonnull<PCB *>> TaskManager::load_init(const char *path) {
        TaskSpec spec{util::owner<TaskMemoryManager *>(nullptr), nullptr,
                      VirAddr(static_cast<addr_t>(0))};
        LoadPrm load_prm{};
        auto preload_res = preload(path, spec, load_prm);
        if (!preload_res.has_value()) {
            loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                     to_cstring(preload_res.error()));
            propagate_return(preload_res);
        }

        // 开始加载程序
        auto load_res = loader::elf::load(spec, load_prm);
        if (!load_res.has_value()) {
            loggers::SUSTCORE::ERROR("加载ELF程序失败! 错误码: %s",
                                     to_cstring(load_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        return create_init_task(spec);
    }

    Result<size_t> TaskManager::lookup_holder_id(pid_t pid) {
        auto pcb_res = _pid_map.get(pid);
        if (!pcb_res.has_value()) {
            unexpect_return(ErrCode::OUT_OF_BOUNDARY);
        }
        PCB *pcb = pcb_res.value().get();
        if (pcb == nullptr || pcb->cholder == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return pcb->cholder->id();
    }

    Result<ForkResult> TaskManager::fork_current() {
        auto *parent_tcb = schd::Scheduler::inst().current_tcb();
        auto *parent_ctx = env::inst().trap_context();
        if (parent_tcb == nullptr || parent_tcb->task == nullptr ||
            parent_ctx == nullptr)
        {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        PCB *parent_pcb = parent_tcb->task;
        if (parent_pcb->tmm == nullptr || parent_pcb->cholder == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }

        auto ret_slot_res = parent_pcb->cholder->internal_lookup_freeslot();
        propagate(ret_slot_res);
        CapIdx ret_slot = ret_slot_res.value();

        auto holder_res = cap::CHolderManager::inst().create_holder();
        propagate(holder_res);
        cap::CHolder *child_holder = holder_res.value();

        auto pgd_res = GFP::get_free_page(1);
        if (!pgd_res.has_value()) {
            cap::CHolderManager::inst().remove_holder(child_holder->id());
            propagate_return(pgd_res);
        }
        auto child_tmm = util::owner(new TaskMemoryManager(pgd_res.value()));

        auto clone_mem_res = parent_pcb->tmm->clone_to_cow(*child_tmm);
        if (!clone_mem_res.has_value()) {
            delete child_tmm;
            cap::CHolderManager::inst().remove_holder(child_holder->id());
            propagate_return(clone_mem_res);
        }

        ErrCode clone_caps_err = ErrCode::SUCCESS;
        parent_pcb->cholder->space().foreach ([&](CapIdx idx,
                                                  cap::Capability *parent_cap) {
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
                child_holder->internal_insert(idx, payload, parent_cap->perm());
            if (!insert_res.has_value()) {
                clone_caps_err = insert_res.error();
            }
        });
        if (clone_caps_err != ErrCode::SUCCESS) {
            delete child_tmm;
            cap::CHolderManager::inst().remove_holder(child_holder->id());
            unexpect_return(clone_caps_err);
        }

        util::nonnull<PCB *> child_pcb = alloc_pcb();
        auto pcb_guard = util::Guard([child_pcb]() { delete child_pcb.get(); });
        TaskSpec spec{child_tmm, child_holder, parent_pcb->entrypoint};
        auto init_res = init_pcb(child_pcb, spec);
        if (!init_res.has_value()) {
            delete child_tmm;
            cap::CHolderManager::inst().remove_holder(child_holder->id());
            propagate_return(init_res);
        }
        child_pcb->pcb_cap      = ret_slot;
        child_pcb->main_tcb_cap = parent_pcb->main_tcb_cap;

        util::nonnull<TCB *> child_tcb = alloc_tcb();
        auto tcb_guard =
            util::Guard([this, child_tcb]() { (void)recycle_tcb(child_tcb); });
        auto init_tcb_res = init_tcb(child_tcb, child_pcb);
        propagate(init_tcb_res);

        *child_tcb->context()                             = *parent_ctx;
        child_tcb->context()->sepc                       += 4;
        child_tcb->context()->regs[Context::A0_BASE]      = ret_slot;
        child_tcb->context()->regs[Context::A0_BASE + 1]  = 0;
        child_tcb->schd_class   = parent_tcb->schd_class;
        child_tcb->basic_entity = {};
        child_tcb->rr_entity    = {};
        child_pcb->threads.push_back(*child_tcb);
        tcb_guard.release();

        auto *pcb_payload = new cap::PCBPayload(child_pcb.get());
        if (pcb_payload == nullptr) {
            unexpect_return(ErrCode::OUT_OF_MEMORY);
        }
        auto insert_parent_res = parent_pcb->cholder->internal_insert(
            ret_slot, pcb_payload, perm::allperm());
        if (!insert_parent_res.has_value()) {
            delete pcb_payload;
            propagate_return(insert_parent_res);
        }
        auto insert_child_res = child_holder->internal_insert(
            ret_slot, pcb_payload, perm::allperm());
        if (!insert_child_res.has_value()) {
            auto remove_res = parent_pcb->cholder->internal_remove(ret_slot);
            assert(remove_res.has_value());
            propagate_return(insert_child_res);
        }

        _pid_map.put(child_pcb->pid, child_pcb.get());
        if (!schd::Scheduler::inst().wakeup_new(child_tcb.get())) {
            _pid_map.remove(child_pcb->pid);
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

        auto slot_res = pcb->cholder->internal_lookup_freeslot();
        propagate(slot_res);
        auto cap_res = pcb->cholder->internal_create<cap::TCBPayload>(
            slot_res.value(), tcb.get());
        propagate(cap_res);
        auto cap_guard = util::Guard([pcb, slot = slot_res.value()]() {
            (void)pcb->cholder->internal_remove(slot);
        });

        if (!schd::Scheduler::inst().wakeup_new(tcb.get())) {
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        cap_guard.release();
        tcb_guard.release();
        return slot_res.value();
    }

    Result<void> TaskManager::exec_current(const char *path,
                                           const CapIdx *reserved_caps,
                                           size_t reserved_count) {
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        if (current_tcb == nullptr || current_tcb->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        return exec_pcb(util::nnullforce(current_tcb->task), path,
                        reserved_caps, reserved_count);
    }

    Result<void> TaskManager::exec_pcb(util::nonnull<PCB *> target,
                                       const char *path,
                                       const CapIdx *reserved_caps,
                                       size_t reserved_count) {
        PCB *pcb = target.get();
        if (pcb->tmm == nullptr || pcb->cholder == nullptr || path == nullptr) {
            unexpect_return(ErrCode::NULLPTR);
        }
        auto *current_tcb = schd::Scheduler::inst().current_tcb();
        bool target_current =
            current_tcb != nullptr && current_tcb->task == pcb;
        schd::ClassType schd_class = schd::ClassType::FCFS;
        TCB *reuse_tcb             = nullptr;
        if (target_current) {
            reuse_tcb   = current_tcb;
            schd_class  = current_tcb->schd_class;
        } else if (!pcb->threads.empty()) {
            schd_class = pcb->threads.front().schd_class;
            if (schd_class == schd::ClassType::IDLE ||
                schd_class == schd::ClassType::BOT)
            {
                schd_class = schd::ClassType::FCFS;
            }
        }

        // 校验 reserved_caps 是否合法
        auto reserved_res = verify_reserved_caps(pcb->cholder, pcb->pcb_cap,
                                                 reserved_caps, reserved_count);
        propagate(reserved_res);

        // 预加载程序到cholder和tmm中, 并构造加载参数
        TaskSpec spec{util::owner<TaskMemoryManager *>(nullptr), nullptr,
                      VirAddr(static_cast<addr_t>(0))};
        LoadPrm load_prm{};
        auto preload_res = preload_into(path, pcb->cholder, spec, load_prm);
        propagate(preload_res);
        bool spec_owned  = true;
        // 使用 RAII 确保在函数退出时正确释放 TaskSpec 中的资源,
        // 避免内存泄漏或资源泄漏
        auto spec_guard  = util::Guard([&]() {
            if (!spec_owned) {
                return;
            }
            if (spec.tmm.get() != nullptr) {
                PhyAddr pgd = spec.tmm->pgd();
                delete spec.tmm;
                GFP::put_page(pgd, 1);
            }
        });
        // 使用 RAII 确保在函数退出时正确释放 Image File 资源,
        // 避免内存泄漏或资源泄漏
        auto image_guard = util::Guard([&]() {
            if (preload_res.has_value()) {
                (void)pcb->cholder->internal_remove(preload_res.value());
            }
        });

        auto load_res = loader::elf::load(spec, load_prm);
        propagate(load_res);

        // TODO: 接下来的操作会对当前进程的内存空间和能力空间进行大幅修改,
        // 应妥善处理执行失败的情况

        // 将 cholder 删减至仅包含 pcb_cap 和 reserved_caps 中的能力索引,
        // 以确保 exec 后不保留不必要的能力
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
        pcb->main_tcb_cap = cap::null;

        auto populate_res =
            populate_task(util::nnullforce(pcb), spec, schd_class, reuse_tcb);
        propagate(populate_res);
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

        if (old_tmm != nullptr) {
            PhyAddr old_pgd = old_tmm->pgd();
            delete old_tmm;
            GFP::put_page(old_pgd, 1);
        }

        loggers::SUSTCORE::INFO("execve成功: path=%s pid=%d", path, pcb->pid);
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
