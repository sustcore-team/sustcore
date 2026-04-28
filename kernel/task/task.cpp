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

#include <arch/riscv64/description.h>
#include <env.h>
#include <exe/elfloader.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/slub.h>
#include <mem/vma.h>
#include <object/csa.h>
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
    Result<PhyAddr> gfp_res = GFP::get_free_page();
    propagate(gfp_res);

    // calculate the top of the kernel stack
    tcb->kstack_phy = gfp_res.value() + TCB::KSTACK_PAGES * PAGESIZE;
    tcb->kstack_top     = convert<KpaAddr>(tcb->kstack_phy).addr();

    // initialization the kstack and context
    void_return();
}

Result<void> TaskManager::init_ctx(util::nonnull<TCB *> tcb, void *entrypoint,
                                   void *stack_top) {
    tcb->context()->pc() = reinterpret_cast<umb_t>(entrypoint);
    tcb->context()->sp() = reinterpret_cast<umb_t>(stack_top);
    tcb->basic_entity    = {};
    tcb->rr_entity       = {};

    tcb->context()->regs[0] = 0x114514;                          // ra设置为0
    tcb->context()->sstatus.spp = 0;                      // 代码运行在 U-Mode
    tcb->context()->sstatus.spie = 1;                     // 用户进程应该开启中断

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
    if (tcb == nullptr) {
        loggers::SUSTCORE::ERROR("分配TCB失败!");
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }

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
        auto term_res = terminate_tcb(util::guarantee_nonnull(&tcb));
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
    if (pcb == nullptr) {
        loggers::SUSTCORE::ERROR("分配PCB失败!");
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }

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
    unexpect_return(ErrCode::NOT_SUPPORTED);
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
    auto gfp_res = GFP::get_free_page();
    if (!gfp_res.has_value()) {
        loggers::SUSTCORE::ERROR("无法为程序页表分配物理页");
        unexpect_return(ErrCode::CREATION_FAILED);
    }
    auto tmm = util::owner(new TaskMemoryManager(gfp_res.value()));

    // 打开文件
    auto *vfs     = env::inst().vfs();
    auto open_res = vfs->open(path);
    propagate(open_res);
    util::owner<VFileAccessor *> file_acc = open_res.value();

    // 加载到CHolder中
    auto csa_res = holder->csa();
    propagate(csa_res);
    CSAOperator csa_op(csa_res.value());
    auto insert_res = csa_op.insert_from<VFileAccessor>(file_acc);
    propagate(insert_res);

    // 设置spec参数
    spec.holder = holder;
    spec.tmm    = tmm;

    // 设置prm参数
    prm.src_path       = path;
    prm.image_file_cap = insert_res.value();
    void_return();
}

Result<util::nonnull<PCB *>> TaskManager::load_elf(const char *path,
                                                   schd::ClassType schd_class) {
    TaskSpec spec;
    LoadPrm load_prm;
    auto preload_res = preload(path, spec, load_prm);
    if (!preload_res.has_value()) {
        loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                 to_cstring(preload_res.error()));
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
    TaskSpec spec;
    LoadPrm load_prm;
    auto preload_res = preload(path, spec, load_prm);
    if (!preload_res.has_value()) {
        loggers::SUSTCORE::ERROR("预加载程序资源失败! 错误码: %s",
                                 to_cstring(preload_res.error()));
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