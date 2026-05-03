/**
 * @file main.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核Main函数
 * @version alpha-1.0.0
 * @date 2025-11-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <arch/riscv64/csr.h>
#include <arch/riscv64/description.h>
#include <arch/riscv64/mem/sv39.h>
#include <arch/trait.h>
#include <cap/capability.h>
#include <cap/cholder.h>
#include <device/block.h>
#include <env.h>
#include <exe/elfloader.h>
#include <exe/task.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/gfp.h>
#include <mem/kaddr.h>
#include <mem/slub.h>
#include <mem/vma.h>
#include <object/csa.h>
#include <perm/csa.h>
#include <perm/permission.h>
#include <sus/baseio.h>
#include <sus/defer.h>
#include <sus/logger.h>
#include <sus/nonnull.h>
#include <sus/path.h>
#include <sus/raii.h>
#include <sus/tree.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <symbols.h>
#include <task/task.h>
#include <test/framework.h>
#include <vfs/ops.h>
#include <vfs/tarfs.h>
#include <vfs/vfs.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace env {
    static Environment _env;
    Environment &inst() {
        return _env;
    }

    void construct() {
        // call the constructor here
        new (&_env) Environment();
    }
}  // namespace env

namespace key {
    using namespace env::key;
    struct main : public tmm, meminfo, vfs, chman, scheduler, tm {
    public:
        main() = default;
    };
}  // namespace key

// path
PhyAddr kernel_root                   = PhyAddr::null;
constexpr const char *INITRD_PATH     = "/initrd/";
constexpr const char *INITMOD_PATH    = "/initrd/init.mod";
constexpr const char *SETUPMOD_PATH   = "/initrd/setup.mod";
constexpr const char *DEFAULTMOD_PATH = "/initrd/default.mod";

util::owner<RamDiskDevice *> make_initrd() {
    auto e_initrd_ptr = reinterpret_cast<char *>(&e_initrd);
    auto s_initrd_ptr = reinterpret_cast<char *>(&s_initrd);
    size_t sz         = e_initrd_ptr - s_initrd_ptr;
    auto device       = util::owner(new RamDiskDevice(&s_initrd, sz, 1));
    loggers::SUSTCORE::INFO("initrd大小为 %u KB =  %u MB", sz / 1024, sz / 1024 / 1024);
    return device;
}

void run_defers(void *s_defer, void *e_defer) {
    auto *s_defer_ptr  = static_cast<util::DeferEntry *>(s_defer);
    auto *e_defer_ptr  = static_cast<util::DeferEntry *>(e_defer);
    size_t defer_count = (e_defer_ptr - s_defer_ptr);
    loggers::SUSTCORE::INFO(
        "开始运行defer构造函数。本批次defer起始地址为%p, 终结于%p, "
        "总共%u个defer",
        s_defer_ptr, e_defer_ptr, defer_count);
    for (size_t i = 0; i < defer_count; i++) {
        auto *entry     = &s_defer_ptr[i];
        bool duplicated = false;
        for (size_t j = 0; j < i; j++) {
            auto *prev = &s_defer_ptr[j];
            if (prev->_instance == entry->_instance &&
                prev->_constructor == entry->_constructor)
            {
                duplicated = true;
                loggers::SUSTCORE::WARN(
                    "跳过重复defer注册: 当前第%d项与第%d项重复, "
                    "defer实例地址为%p, 构造器为%p",
                    i, j, entry->_instance, entry->_constructor);
                break;
            }
        }
        if (duplicated) {
            continue;
        }
        loggers::SUSTCORE::DEBUG(
            "运行第%d个defer构造函数, defer实例地址为%p, 构造器为%p", i,
            entry->_instance, entry->_constructor);
        entry->_constructor(entry->_instance);
    }
}

void kernel_paging_setup() {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    auto &e                     = env::inst();
    // 创建内核页表管理器
    auto gfp_res                = GFP::get_free_page<STAGE>(1);
    if (!gfp_res.has_value()) {
        loggers::SUSTCORE::ERROR("无法为内核页表分配物理页");
        while (true);
    }

    PhyAddr pgd = gfp_res.value();
    EarlyPageMan::make_root(pgd);
    kernel_root = pgd;
    EarlyPageMan kernelman(kernel_root);

    ker_paddr::init();
    ker_paddr::mapping_kernel_areas(kernelman);

    // 对[0, uppm)进行恒等映射
    size_t sz = e.meminfo().uppm - e.meminfo().lowpm;
    kernelman.map_range<true>(e.meminfo().lowvm, e.meminfo().lowpm, sz,
                              EarlyPageMan::rwx(true, true, true), false, true);

    kernelman.switch_root();
    kernelman.flush_tlb();
}

Result<void> init_vfs() {
    auto &e = env::inst();

    // 构造VFS
    auto vfs           = new VFS();
    e.vfs(key::main()) = vfs;

    // 加载驱动程序
    auto tarfs        = util::owner(new tarfs::TarFSDriver());
    auto register_res = vfs->register_fs(tarfs);
    propagate(register_res);

    auto initrd_device = make_initrd();
    auto mount_res     = vfs->mount("tarfs", initrd_device, INITRD_PATH,
                                    MountFlags::NONE, nullptr);
    propagate(mount_res);
    void_return();
}

Result<void> init_tm() {
    auto &e           = env::inst();
    e.tm(key::main()) = new TaskManager();
    void_return();
}

Result<void> init_scheduler() {
    auto &e       = env::inst();
    auto load_res = e.tm()->load_init(INITMOD_PATH);
    if (!load_res.has_value()) {
        loggers::SUSTCORE::ERROR("加载初始进程失败! 错误码: %s",
                                 to_cstring(load_res.error()));
        propagate_return(load_res);
    }

    auto task = load_res.value();
    assert(task->threads.size() == 1);
    e.scheduler(key::main()) =
        new schd::Scheduler(task->threads.front());
    e.scheduler()->init();
    void_return();
}


extern "C" void post_init(void) {
    loggers::SUSTCORE::INFO("已进入 post-init 阶段");
    auto &e = env::inst();

    // 将 pre-init 阶段中初始化的子系统再次初始化, 以适应内核虚拟地址空间
    GFP::post_init();
    PageMan::init();

    Allocator::init();

    // Allocator初始化后, 系统的基本功能已经可用
    // 此时可以执行绝大部分的构造函数, 因此在此时调用defer初始化
    run_defers(&s_defer, &e_defer);

    // 初始化中断处理程序
    Interrupt::init();
    // 按理来说这个东西应该放在下面的
    // 但是我忘记把device tree给提升到kernel physical address space中了
    // 我现在也改不动了
    // 就先放在这吧
    Initialization::post_init();

    // 将低端内存设置为用户态
    auto &meminfo = e.meminfo();
    PageMan kernelman(kernel_root);
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        meminfo.lowvm, meminfo.uppm - meminfo.lowpm, PageMan::RWX::NONE, true,
        false);

    e.chman(key::main()) = new CHolderManager();

    auto init_res = init_vfs();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::ERROR("初始化VFS失败! 错误码: %s",
                                 to_cstring(init_res.error()));
        while (true);
    }

    init_res = init_tm();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::ERROR("初始化TaskManager失败! 错误码: %s",
                                 to_cstring(init_res.error()));
        while (true);
    }

    init_res = init_scheduler();
    if (!init_res.has_value()) {
        loggers::SUSTCORE::ERROR("初始化Scheduler失败! 错误码: %s",
                                 to_cstring(init_res.error()));
        while (true);
    }

    // 打开中断
    Interrupt::sti();

    // Kernel tests
#ifdef __CONF_KERNEL_TESTS
    TestFramework framework;
    collect_tests(framework);
    framework.run_all();
#else
#endif

    loggers::SUSTCORE::INFO("Test complete. Entering idle loop.");

    while (true);
}

extern "C" void redive(void);

void pre_init() {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    // construct the env
    env::construct();

    Initialization::pre_init();

    auto &e         = env::inst();
    auto detect_res = MemoryLayout::detect();

    if (!detect_res.has_value()) {
        loggers::SUSTCORE::FATAL("探测内存区域失败!错误码: %s",
                                 to_cstring(detect_res.error()));
        while (true);
    }

    PhyAddr upper_bound = PhyAddr::null;
    for (int i = 0; i < e.meminfo().region_cnt; i++) {
        const auto &reg = e.meminfo().regions[i];
        PhyAddr start   = reg.ptr;
        PhyAddr end     = start + reg.size;

        loggers::SUSTCORE::INFO("探测到内存区域 %d: [%p, %p) Status: %d", i,
                                start.addr(), end.addr(),
                                static_cast<int>(reg.status));
        if (upper_bound < end) {
            upper_bound = end;
        }
    }
    e.meminfo(key::main()).uppm = upper_bound;

    loggers::SUSTCORE::INFO("初始化GFP");
    GFP::pre_init();

    loggers::SUSTCORE::INFO("初始化内核地址空间管理器");
    EarlyPageMan::init();

    kernel_paging_setup();

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    // 将 redive() 函数定位到KvaAddr
    typedef void (*RediveFuncType)(void);
    auto redive_paddr  = (PhyAddr)(void *)redive;
    auto redive_kvaddr = convert<KvaAddr>(redive_paddr);
    loggers::SUSTCORE::DEBUG("redive函数物理地址: %p, 内核虚拟地址: %p",
                             redive_paddr.addr(), redive_kvaddr.addr());
    auto redive_func = (RediveFuncType)redive_kvaddr.addr();
    loggers::SUSTCORE::DEBUG("跳转到内核虚拟地址空间中的redive函数: %p",
                             redive_func);
    redive_func();
    loggers::SUSTCORE::ERROR("redive函数返回了, 这不应该发生!");
    while (true);
}

void kernel_setup(void) {
    pre_init();
    while (true);
}