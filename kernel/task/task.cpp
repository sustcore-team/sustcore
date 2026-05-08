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

#include <env.h>
#include <exe/elfloader.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <mem/vma.h>
#include <perm/permission.h>
#include <schd/schdbase.h>
#include <sus/defer.h>
#include <sus/nonnull.h>
#include <sus/raii.h>
#include <sustcore/addr.h>
#include <sustcore/errcode.h>
#include <task/task.h>
#include <task/task_struct.h>

namespace key {
    using namespace env::key;
    struct task : public tmm {
    public:
        task() = default;
    };
}  // namespace key

Result<void> TaskManager::init_tcb(util::nonnull<TCB *> tcb,
                                   util::nonnull<PCB *> task /* ... args*/) {
    // 初始化 TCB 基本信息
    tcb->tid       = alloc_tid();
    tcb->task      = task;
    tcb->list_head = {};

    // ask for a kstack for this thread
    Result<PhyAddr> gfp_res = GFP::get_free_page(TCB::KSTACK_PAGES);
    propagate(gfp_res);

    // calculate the top of the kernel stack
    tcb->kstack_phy = gfp_res.value() + TCB::KSTACK_PAGES * PAGESIZE;
    tcb->kstack_top = convert<KpaAddr>(tcb->kstack_phy).addr();

    // initialization the kstack and context
    void_return();
}

Result<void> TaskManager::init_ctx(util::nonnull<TCB *> tcb, void *entrypoint,
                                   void *stack_top) {
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

    // initialize the pcb according to the specification
    pcb->tmm        = spec.tmm;
    pcb->cholder    = spec.holder;
    pcb->entrypoint = spec.entrypoint;
    void_return();
}

Result<void> TaskManager::construct_thread(util::nonnull<PCB *> pcb,
                                           void *entrypoint, void *stack_top,
                                           schd::ClassType schd_class) {
    util::nonnull<TCB *> tcb = alloc_tcb();
    auto tcb_guard = util::Guard([this, tcb]() { tcb_pool.free(tcb); });
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
    void_return();
}

Result<void> TaskManager::construct_main_thread(util::nonnull<PCB *> pcb,
                                                schd::ClassType schd_class) {
    // 为主线程分配初始栈空间, 并将其加入Task Memory的VMA中
    // 此处无需通过GFP分配物理页, 由缺页中断自动处理即可
    auto vma_res =
        pcb->tmm->add_vma(VMA::Type::STACK, USER_STACK_BOTTOM, USER_STACK_TOP);

    return construct_thread(pcb, pcb->entrypoint.addr(), USER_STACK_TOP.addr(),
                            schd_class);
}

Result<void> TaskManager::terminate_tcb(util::nonnull<TCB *> tcb) {
    // free the kstack
    GFP::put_page(tcb->kstack_phy, TCB::KSTACK_PAGES);
    tcb_pool.free(tcb);
    void_return();
}

Result<void> TaskManager::terminate_pcb(util::nonnull<PCB *> pcb) {
    // terminate all threads in this process
    for (auto &tcb : pcb->threads) {
        auto term_res = terminate_tcb(util::nnullforce(&tcb));
        if (!term_res.has_value()) {
            loggers::SUSTCORE::ERROR("终止线程 %d 失败! 错误码: %s", tcb.tid,
                                     to_cstring(term_res.error()));
            propagate_return(term_res);
        }
    }

    // free the tmm
    delete pcb->tmm;
    // ask cholder manager to remove the cholder
    auto chman  = env::inst().chman();
    auto rm_res = chman->remove_holder(pcb->cholder->id());
    propagate(rm_res);

    pcb_pool.free(pcb);

    void_return();
}

Result<util::nonnull<PCB *>> TaskManager::create_init_task(
    TaskSpec spec /* ... args*/) {
    constexpr schd::ClassType INIT_SCHED_CLASS = schd::ClassType::IDLE;
    util::nonnull<PCB *> pcb                   = alloc_pcb();
    auto pcb_guard = util::Guard([this, pcb]() { pcb_pool.free(pcb); });

    auto init_res = init_pcb(pcb, spec);
    if (!init_res.has_value()) {
        loggers::SUSTCORE::ERROR("初始化PCB失败! 错误码: %s",
                                 to_cstring(init_res.error()));
        propagate_return(init_res);
    }

    auto main_thread_res = construct_main_thread(pcb, INIT_SCHED_CLASS);
    if (!main_thread_res.has_value()) {
        loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                 to_cstring(main_thread_res.error()));
        propagate_return(main_thread_res);
    }

    pcb_guard.release();  // 进程已成功构造，释放PCB的自动释放机制
    return pcb;
}

Result<util::nonnull<PCB *>> TaskManager::create_task(
    TaskSpec spec, schd::ClassType schd_class /* ... args*/) {
    util::nonnull<PCB *> pcb = alloc_pcb();
    auto pcb_guard = util::Guard([this, pcb]() { pcb_pool.free(pcb); });

    auto init_res = init_pcb(pcb, spec);
    if (!init_res.has_value()) {
        loggers::SUSTCORE::ERROR("初始化PCB失败! 错误码: %s",
                                 to_cstring(init_res.error()));
        propagate_return(init_res);
    }

    auto main_thread_res = construct_main_thread(pcb, schd_class);
    if (!main_thread_res.has_value()) {
        loggers::SUSTCORE::ERROR("构造主线程失败! 错误码: %s",
                                 to_cstring(main_thread_res.error()));
        propagate_return(main_thread_res);
    }

    auto *scheduler = env::inst().scheduler();
    if (scheduler == nullptr) {
        loggers::SUSTCORE::ERROR("调度器尚未初始化, 无法唤醒新进程");
        unexpect_return(ErrCode::CREATION_FAILED);
    }

    TCB *main_tcb = &pcb->threads.front();
    if (!scheduler->wakeup_new(main_tcb)) {
        loggers::SUSTCORE::ERROR("唤醒新进程失败");
        unexpect_return(ErrCode::CREATION_FAILED);
    }

    pcb_guard.release();  // 进程已成功构造并加入调度队列
    return pcb;
}

Result<void> TaskManager::preload(const char *path, TaskSpec &spec,
                                  LoadPrm &prm) {
    auto &e = env::inst();

    // 构造cholder
    auto create_res = e.chman()->create_holder();
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
    auto *vfs     = env::inst().vfs();
    auto open_res = vfs->open(path);
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

Result<util::nonnull<PCB *>> TaskManager::load_elf(const char *path,
                                                   schd::ClassType schd_class) {
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
