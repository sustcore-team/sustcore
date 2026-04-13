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
#include <cap/cspace.h>
#include <device/block.h>
#include <env.h>
#include <fs/tarfs.h>
#include <kio.h>
#include <mem/addr.h>
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
#include <sus/path.h>
#include <sus/raii.h>
#include <sus/tree.h>
#include <sus/types.h>
#include <sustcore/addr.h>
#include <symbols.h>
#include <task/task.h>
#include <test/framework.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <unordered_map>

namespace env {
    static Environment _env;
    Environment &inst() {
        return _env;
    }
    
    void construct()
    {
        // call the constructor here
        new(&_env) Environment();
    }

    TM *Environment::tm() const {
        return this->_tm;
    }
    TM *&Environment::tm(key::tm) {
        return this->_tm;
    }

    PhyAddr Environment::pgd() const {
        return PageMan::read_root();
    }

    const MemInfo &Environment::meminfo() const
    {
        return _meminfo;
    }

    MemInfo &Environment::meminfo(key::meminfo)
    {
        return _meminfo;
    }
}

namespace key {
    struct main : public env::key::tm, env::key::meminfo
    {
    public:
        main() = default;
    };
}

// path
PhyAddr kernel_root = PhyAddr::null;
constexpr const char *initrd_path   = "/initrd";
constexpr const char *setupmod_path = "mods/setup.mod";

RamDiskDevice *make_initrd(void) {
    size_t sz             = (char *)&e_initrd - (char *)&s_initrd;
    RamDiskDevice *device = new RamDiskDevice(&s_initrd, sz, 1);
    loggers::SUSTCORE::INFO("initrd大小为 %u KB", sz / 1024, sz / 1024 / 1024);
    return device;
}

void run_defers(void *s_defer, void *e_defer) {
    constexpr size_t DEFER_ENTRY_SIZE = sizeof(util::DeferEntry);
    size_t defer_seg_size             = (char *)e_defer - (char *)s_defer;
    assert(defer_seg_size % DEFER_ENTRY_SIZE == 0);
    size_t defer_count = defer_seg_size / DEFER_ENTRY_SIZE;
    loggers::SUSTCORE::INFO(
        "开始运行defer构造函数。本批次defer起始地址为%p, 终结于%p, "
        "总共%u个defer",
        s_defer, e_defer, defer_count);
    for (size_t i = 0; i < defer_count; i++) {
        util::DeferEntry *entry =
            (util::DeferEntry *)((uintptr_t)s_defer + i * DEFER_ENTRY_SIZE);
        bool duplicated = false;
        for (size_t j = 0; j < i; j++) {
            util::DeferEntry *prev =
                (util::DeferEntry *)((uintptr_t)s_defer + j * DEFER_ENTRY_SIZE);
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

void kernel_paging_setup(void) {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    auto &e = env::inst();
    // 创建内核页表管理器
    auto gfp_res                = GFP::get_free_page<STAGE>();
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

    Interrupt::init();
    Initialization::post_init();

    // 将低端内存设置为用户态
    auto &meminfo = e.meminfo();
    PageMan kernelman(kernel_root);
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        meminfo.lowvm, meminfo.uppm - meminfo.lowpm, PageMan::RWX::NONE, true, false);

    // Kernel tests
#ifdef __CONF_KERNEL_TESTS
    TestFramework framework;
    collect_tests(framework);
    framework.run_all();
#endif

    auto gfp_res = GFP::get_free_page<KernelStage::POST_INIT>();
    if (!gfp_res.has_value()) {
        loggers::SUSTCORE::ERROR("无法为 TM 分配物理页");
        while (true);
    }
    PhyAddr tm_pgd = gfp_res.value();
    TM *tm         = new TM(tm_pgd);

    constexpr VirAddr data_vaddr = VirAddr(0x0000'0000'9000'0000);
    constexpr size_t data_size   = 0x0000'0000'1000'0000;  // 256MB

    Result<void> add_res = tm->add_vma(VMA::Type::DATA, data_vaddr, data_size);
    if (!add_res.has_value()) {
        loggers::SUSTCORE::ERROR("无法添加VMA: %d", add_res.error());
        while (true);
    }

    // 切换当前页表到 TM 管理的页表
    tm->pman().switch_root();
    tm->pman().flush_tlb();
    env::inst().tm(key::main()) = tm;

    // 调试: 检查当前页表根一致性
    PhyAddr tm_root  = tm->pgd();
    loggers::SUSTCORE::DEBUG("TM 切换完成: hw_root=%p, tm_root=%p",
                             env::inst().pgd().addr(), tm_root.addr());
    if (env::inst().pgd() != tm_root) {
        loggers::SUSTCORE::ERROR("页表根不一致!");
        while (true);
    }

    // try access pages in the new VMA to trigger on_np

    // first, allow the kernel to directly read/write user memory
    // it'll be removed later

    csr_sstatus_t sstatus = csr_get_sstatus();
    sstatus.sum           = 1;  // 允许S-MODE访问U-MODE内存
    csr_set_sstatus(sstatus);

    volatile char *ptr              = (volatile char *)data_vaddr.addr();
    constexpr char THE_FINAL_ANSWER = 0x42;
    // write the page
    for (int i = 0; i < 8 * PAGESIZE; i++) {
        ptr[i] = THE_FINAL_ANSWER;
    }

    sstatus     = csr_get_sstatus();
    sstatus.sum = 0;
    csr_set_sstatus(sstatus);

    loggers::SUSTCORE::INFO("Test complete. Entering idle loop.");

    while (true);
}

extern "C" void redive(void);

void pre_init(void) {
    [[maybe_unused]]
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    // construct the env
    env::construct();

    Initialization::pre_init();

    auto &e = env::inst();
    auto detect_res = MemoryLayout::detect();

    if (! detect_res.has_value())
    {
        loggers::SUSTCORE::FATAL("探测内存区域失败!错误码: %s", to_cstring(detect_res.error()));
        while (true);
    }

    PhyAddr upper_bound = PhyAddr::null;
    for (int i = 0; i < e.meminfo().region_cnt; i++) {
        const auto &reg = e.meminfo().regions[i];
        PhyAddr start = reg.ptr;
        PhyAddr end   = start + reg.size;

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
    PhyAddr redive_paddr  = (PhyAddr)(void *)redive;
    KvaAddr redive_kvaddr = convert<KvaAddr>(redive_paddr);
    loggers::SUSTCORE::DEBUG("redive函数物理地址: %p, 内核虚拟地址: %p",
                             redive_paddr.addr(), redive_kvaddr.addr());
    RediveFuncType redive_func = (RediveFuncType)redive_kvaddr.addr();
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