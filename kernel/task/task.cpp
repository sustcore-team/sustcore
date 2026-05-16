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
#include <env.h>
#include <exe/elfloader.h>
#include <logger.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <mem/vma.h>
#include <object/task.h>
#include <perm/permission.h>
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
        auto slot_res = pcb->cholder->internal_lookup_freeslot();
        propagate(slot_res);
        auto create_res = pcb->cholder->internal_create<cap::PCBPayload>(
            slot_res.value(), pcb);
        propagate(create_res);
        return slot_res.value();
    }

    Result<void> TaskManager::init_tcb(
        util::nonnull<TCB *> tcb, util::nonnull<PCB *> task /* ... args*/) {
        // 初始化 TCB 基本信息
        tcb->tid         = alloc_tid();
        tcb->task        = task;
        tcb->list_head   = {};
        tcb->wait_reason = 0;
        tcb->wait_head   = {};

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

        void_return();
    }

    Result<void> TaskManager::init_pcb(util::nonnull<PCB *> pcb,
                                       TaskSpec spec /* ... args*/) {
        pcb->pid     = alloc_pid();
        pcb->threads = {};
        pcb->exiting = false;
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
        auto tcb_guard = util::Guard([tcb]() { delete tcb.get(); });
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
        tcb_guard.release();  // 线程已成功构造，释放TCB的自动释放机制
        return tcb;
    }

    Result<util::nonnull<TCB *>> TaskManager::construct_main_thread(
        util::nonnull<PCB *> pcb, schd::ClassType schd_class,
        task::StartupInfo startup_info) {
        // 为主线程分配初始栈空间, 并将其加入Task Memory的VMA中
        // 此处无需通过GFP分配物理页, 由缺页中断自动处理即可
        auto vma_res =
            pcb->tmm->add_vma(VMA::Type::STACK, VMA::Growth::GROW_DOWN,
                              VirArea(USER_STACK_BOTTOM, USER_STACK_TOP));
        propagate(vma_res);

        auto con_res = construct_thread(pcb, pcb->entrypoint.addr(),
                                        USER_STACK_TOP.addr(), schd_class);
        propagate(con_res);
        util::nonnull<TCB *> tcb = con_res.value();

        auto tcb_cap_slot_res = pcb->cholder->internal_lookup_freeslot();
        propagate(tcb_cap_slot_res);
        auto tcb_cap_res = pcb->cholder->internal_create<cap::TCBPayload>(
            tcb_cap_slot_res.value(), tcb.get());
        propagate(tcb_cap_res);

        pcb->main_tcb_cap         = tcb_cap_slot_res.value();
        startup_info.pcb_cap      = pcb->pcb_cap;
        startup_info.main_tcb_cap = pcb->main_tcb_cap;
        tcb->context()->write_startup(startup_info);

        return tcb;
    }

    Result<void> TaskManager::terminate_tcb(util::nonnull<TCB *> tcb) {
        PCB *pcb = tcb->task;
        if (pcb != nullptr) {
            pcb->threads.remove(*tcb);
        }
        // free the kstack
        GFP::put_page(tcb->kstack_phy - TCB::KSTACK_SIZE, TCB::KSTACK_PAGES);
        delete tcb.get();
        void_return();
    }

    Result<void> TaskManager::terminate_pcb(util::nonnull<PCB *> pcb) {
        // terminate all threads in this process
        while (!pcb->threads.empty()) {
            TCB *tcb = &pcb->threads.front();
            auto term_res = terminate_tcb(util::nnullforce(tcb));
            if (!term_res.has_value()) {
                loggers::SUSTCORE::ERROR("终止线程 %d 失败! 错误码: %s",
                                         tcb->tid, to_cstring(term_res.error()));
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

    Result<void> TaskManager::exit_current() {
        TCB *current_tcb = schd::Scheduler::inst().current_tcb();
        if (current_tcb == nullptr || current_tcb->task == nullptr) {
            unexpect_return(ErrCode::INVALID_PARAM);
        }
        PCB *pcb = current_tcb->task;
        if (pcb->exiting) {
            void_return();
        }
        pcb->exiting = true;

        if (pcb->cholder != nullptr) {
            pcb->cholder->internal_clear();
        }

        for (auto &tcb : pcb->threads) {
            if (&tcb != current_tcb &&
                tcb.basic_entity.state == ThreadState::READY)
            {
                auto dequeue_res =
                    schd::Scheduler::inst().dequeue(util::nnullforce(&tcb));
                if (!dequeue_res.has_value()) {
                    loggers::SUSTCORE::ERROR(
                        "退出进程时移除线程 %d 失败! 错误码: %s", tcb.tid,
                        to_cstring(dequeue_res.error()));
                }
            }
            tcb.basic_entity.state = ThreadState::WAITING;
        }

        current_tcb->basic_entity.state = ThreadState::WAITING;
        current_tcb->basic_entity
            .template flags_set<schd::SchedMeta::FLAGS_NEED_RESCHED>();
        schd::Scheduler::inst().schedule();
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

        auto pcb_cap_res = insert_pcb_cap(pcb);
        propagate(pcb_cap_res);
        pcb->pcb_cap = pcb_cap_res.value();

        task::StartupInfo startup_info{
            .heap_vaddr   = spec.heap_vaddr,
            .stack_vaddr  = USER_STACK_BOTTOM,
            .entrypoint   = spec.entrypoint,
            .pcb_cap      = pcb->pcb_cap,
            .main_tcb_cap = cap::null,
        };
        auto main_thread_res =
            construct_main_thread(pcb, INIT_SCHED_CLASS, startup_info);
        if (!main_thread_res.has_value()) {
            loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                     to_cstring(main_thread_res.error()));
            propagate_return(main_thread_res);
        }

        _pid_map.put(pcb->pid, pcb);
        pcb_guard.release();  // 进程已成功构造，释放PCB的自动释放机制
        return pcb;
    }

    Result<util::nonnull<PCB *>> TaskManager::create_task(
        TaskSpec spec, schd::ClassType schd_class /* ... args*/) {
        util::nonnull<PCB *> pcb = alloc_pcb();
        auto pcb_guard = util::Guard([pcb]() { delete pcb.get(); });

        auto init_res = init_pcb(pcb, spec);
        if (!init_res.has_value()) {
            loggers::SUSTCORE::ERROR("初始化PCB失败! 错误码: %s",
                                     to_cstring(init_res.error()));
            propagate_return(init_res);
        }

        auto pcb_cap_res = insert_pcb_cap(pcb);
        propagate(pcb_cap_res);
        pcb->pcb_cap = pcb_cap_res.value();

        task::StartupInfo startup_info{
            .heap_vaddr   = spec.heap_vaddr,
            .stack_vaddr  = USER_STACK_BOTTOM,
            .entrypoint   = spec.entrypoint,
            .pcb_cap      = pcb->pcb_cap,
            .main_tcb_cap = cap::null,
        };
        auto main_thread_res =
            construct_main_thread(pcb, schd_class, startup_info);
        if (!main_thread_res.has_value()) {
            loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                     to_cstring(main_thread_res.error()));
            propagate_return(main_thread_res);
        }

        TCB *main_tcb = &pcb->threads.front();
        if (!schd::Scheduler::inst().wakeup_new(main_tcb)) {
            loggers::SUSTCORE::ERROR("唤醒新进程失败");
            unexpect_return(ErrCode::CREATION_FAILED);
        }

        _pid_map.put(pcb->pid, pcb);
        pcb_guard.release();  // 进程已成功构造并加入调度队列
        return pcb;
    }

    Result<void> TaskManager::preload(const char *path, TaskSpec &spec,
                                      LoadPrm &prm) {
        // 构造cholder
        auto create_res = cap::CHolderManager::inst().create_holder();
        if (!create_res.has_value()) {
            loggers::SUSTCORE::ERROR("创建CHolder失败! 错误码: %s",
                                     to_cstring(create_res.error()));
            unexpect_return(ErrCode::CREATION_FAILED);
        }
        auto holder = create_res.value();

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
        auto slot_res = holder->internal_lookup_freeslot();
        if (!slot_res.has_value()) {
            file->destruct();
            propagate_return(slot_res);
        }
        auto insert_res = holder->internal_insert(slot_res.value(), file);
        if (!insert_res.has_value()) {
            file->destruct();
            propagate_return(insert_res);
        }

        // 设置spec参数
        spec.holder = holder;
        spec.tmm    = tmm;

        // 设置prm参数
        prm.src_path       = path;
        prm.image_file_cap = slot_res.value();
        void_return();
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

        auto clone_caps_res = parent_pcb->cholder->internal_copy_all_to(*child_holder);
        if (!clone_caps_res.has_value()) {
            delete child_tmm;
            cap::CHolderManager::inst().remove_holder(child_holder->id());
            propagate_return(clone_caps_res);
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
        auto tcb_guard = util::Guard([child_tcb]() { delete child_tcb.get(); });
        auto init_tcb_res = init_tcb(child_tcb, child_pcb);
        propagate(init_tcb_res);

        *child_tcb->context() = *parent_ctx;
        child_tcb->context()->sepc += 4;
        child_tcb->context()->regs[Context::A0_BASE]     = ret_slot;
        child_tcb->context()->regs[Context::A0_BASE + 1] = 0;
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
        auto insert_child_res =
            child_holder->internal_insert(ret_slot, pcb_payload, perm::allperm());
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
