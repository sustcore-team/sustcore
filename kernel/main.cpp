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

#include <arch/riscv64/description.h>
#include <arch/riscv64/mem/sv39.h>
#include <arch/trait.h>
#include <cap/capability.h>
#include <cap/capcall.h>
#include <cap/permission.h>
#include <device/block.h>
#include <event/init_events.h>
#include <event/registries.h>
#include <fs/tarfs.h>
#include <kio.h>
#include <mem/addr.h>
#include <mem/alloc.h>
#include <mem/gfp.h>
#include <mem/kaddr.h>
#include <mem/slub.h>
#include <sus/baseio.h>
#include <sus/defer.h>
#include <sus/logger.h>
#include <sus/raii.h>
#include <sus/tree.h>
#include <sus/types.h>
#include <symbols.h>
#include <task/task.h>
#include <vfs/ops.h>
#include <vfs/vfs.h>

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

void buddy_test_complex(void);
void slub_test_basic(void);
void tree_test(void);
void fs_test(void);

int kputs(const char *str) {
    size_t len        = strlen(str);
    PhyAddr str_paddr = convert_pointer(str);
    Serial::serial_write_string(len, str_paddr.as<char>());
    return strlen(str);
}

int kputchar(char ch) {
    Serial::serial_write_char(ch);
    return ch;
}

char kgetchar() {
    return '\0';
}

int KernelIO::putchar(char c) {
    return kputchar(c);
}

int KernelIO::puts(const char *str) {
    return kputs(str);
}

char KernelIO::getchar() {
    return kgetchar();
}

int kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vbprintf<KernelIO>(fmt, args);
    va_end(args);
    return len;
}

RamDiskDevice *make_initrd(void) {
    size_t sz             = (char *)&e_initrd - (char *)&s_initrd;
    RamDiskDevice *device = new RamDiskDevice(&s_initrd, sz, 1);
    LOGGER::INFO("initrd大小为 %u KB", sz / 1024, sz / 1024 / 1024);
    return device;
}

MemRegion regions[128];
size_t region_cnt   = 0;
PhyAddr kernel_root = PhyAddr::null;
const PhyAddr lowpm = PhyAddr::null;
PhyAddr uppm        = PhyAddr::null;
const VirAddr lowvm = VirAddr::null;

void run_defers(void *s_defer, void *e_defer) {
    constexpr size_t DEFER_ENTRY_SIZE = sizeof(util::DeferEntry);
    size_t defer_seg_size             = (char *)e_defer - (char *)s_defer;
    assert(defer_seg_size % DEFER_ENTRY_SIZE == 0);
    size_t defer_count = defer_seg_size / DEFER_ENTRY_SIZE;
    LOGGER::INFO(
        "开始运行defer构造函数。本批次defer起始地址为%p, 终结于%p, "
        "总共%u个defer",
        s_defer, e_defer, defer_count);
    for (size_t i = 0; i < defer_count; i++) {
        util::DeferEntry *entry =
            (util::DeferEntry *)((uintptr_t)s_defer + i * DEFER_ENTRY_SIZE);
        LOGGER::DEBUG("运行第%d个defer构造函数, defer实例地址为%p, 构造器为%p",
                      i, entry->_instance, entry->_constructor);
        entry->_constructor(entry->_instance);
    }
}

void kernel_paging_setup(void) {
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    // 创建内核页表管理器
    kernel_root                 = EarlyPageMan::make_root();
    EarlyPageMan kernelman(kernel_root);

    ker_paddr::init(lowpm, uppm);
    ker_paddr::mapping_kernel_areas(kernelman);

    // 对[0, uppm)进行恒等映射
    size_t sz = uppm - lowpm;
    kernelman.map_range<true>(lowvm, lowpm, sz,
                              EarlyPageMan::rwx(true, true, true), false, true);

    kernelman.switch_root();
    kernelman.flush_tlb();
}

extern "C" void post_init(void) {
    // logger
    LOGGER::INFO("已进入 post-init 阶段");

    // 将 pre-init 阶段中初始化的子系统再次初始化, 以适应内核虚拟地址空间
    GFP::post_init(regions, region_cnt);
    PageMan::init();

    // 初始化默认 Allocator 子系统
    Allocator::init();

    // 发布PostGlobalObjectInitEvent
    // 收集全局对象Defer并执行构造
    PostGlobalObjectInitEvent pre_init_event;
    EventDispatcher<PostGlobalObjectInitEvent>::dispatch(pre_init_event);
    run_defers(&s_defer_post, &e_defer_post);

    // 初始化中断
    Interrupt::init();

    // 架构后初始化
    Initialization::post_init();

    // 将低端内存设置为用户态

    PageMan kernelman(kernel_root);
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        lowvm, uppm - lowpm, PageMan::RWX::NONE, true, false);

    TCBManager::init();

    buddy_test_complex();
    slub_test_basic();
    fs_test();

    for (size_t i = 0; i < 10; i++) {
        TCB *t = new TCB();
        PCB *p = new PCB();
        kprintf("%p;%p\n", t, p);
    }

    while (true);
}

extern "C" void redive(void);

void pre_init(void) {
    constexpr KernelStage STAGE = KernelStage::PRE_INIT;
    Initialization::pre_init();

    memset(regions, 0, sizeof(regions));
    region_cnt          = MemoryLayout::detect_memory_layout(regions, 128);
    PhyAddr upper_bound = PhyAddr::null;
    for (int i = 0; i < region_cnt; i++) {
        PhyAddr start = regions[i].ptr;
        PhyAddr end   = start + regions[i].size;

        LOGGER::INFO("探测到内存区域 %d: [%p, %p) Status: %d", i, start.addr(),
                     end.addr(), static_cast<int>(regions[i].status));
        if (upper_bound < end) {
            upper_bound = end;
        }
    }

    // 发布PreGlobalObjectInitEvent
    PreGlobalObjectInitEvent pre_init_event;
    EventDispatcher<PreGlobalObjectInitEvent>::dispatch(pre_init_event);
    run_defers(&s_defer_pre, &e_defer_pre);

    LOGGER::INFO("初始化GFP");
    GFP::pre_init(regions, region_cnt);

    LOGGER::INFO("初始化内核地址空间管理器");
    EarlyPageMan::init();
    uppm = upper_bound;
    kernel_paging_setup();

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    typedef void (*RediveFuncType)(void);
    PhyAddr redive_paddr  = (PhyAddr)(void *)redive;
    KvaAddr redive_kvaddr = convert<KvaAddr>(redive_paddr);
    LOGGER::DEBUG("redive函数物理地址: %p, 内核虚拟地址: %p",
                  redive_paddr.addr(), redive_kvaddr.addr());
    RediveFuncType redive_func = (RediveFuncType)redive_kvaddr.addr();
    LOGGER::DEBUG("跳转到内核虚拟地址空间中的redive函数: %p", redive_func);
    redive_func();
    LOGGER::ERROR("redive函数返回了, 这不应该发生!");
    while (true);
}

void cap_test(void) {
    // 这部分的测试仅仅是为了测试编译有效性
    CapHolder holder;
    auto cap_opt = holder.lookup<CSpaceBase>(CapIdx(0));
    // 绝大多数情况下, 你都不应该这么做来解包
    // 此处只是用于测试编译有效性
    // 可读性为0
    auto sp_opt  = cap_opt.and_then_opt(
        // 第一个 lambda 为
        // Capability<CSpaceBase> * -> CapOptional<CSpace<CSpaceBase> *>
        [](auto *cap) {
            return CSpaceCalls::basic::payload(cap).and_then(
                // 第二个 lambda 为 CSpaceBase * -> CSpace<CSpaceBase> *
                [](CSpaceBase *base) {
                    return base->as<CSpace<CSpaceBase>>();
                });
        });
    if (sp_opt.present()) {
        CSpace<CSpaceBase> *sp = sp_opt.value();
        LOGGER::INFO("成功获取 CSpace: %p", sp);
    } else {
        LOGGER::INFO("未能获取 CSpace");
        return;
    }

    Capability<CSpaceBase> *cap = cap_opt.value();
    size_t space_idx            = CSpaceCalls::index(cap);
    size_t slot = CSpaceCalls::alloc<CSpaceBase>(cap).or_else(0);
    if (slot == 0) {
        LOGGER::ERROR("分配槽位slot失败");
        return;
    }
    CSpaceCalls::insert(cap, slot, util::make_raii(cap));
    CSpaceCalls::remove<CSpaceBase>(cap, slot);
    CSpaceCalls::insert(cap, slot, util::make_raii(cap));
    size_t slot2 = CSpaceCalls::alloc<CSpaceBase>(cap).or_else(0);
    if (slot == 0) {
        LOGGER::ERROR("分配槽位slot2失败");
        return;
    }
    CSpaceCalls::clone<CSpaceBase>(cap, CapIdx(space_idx, slot2), &holder,
                                   CapIdx(space_idx, slot));
    // to check the compiler errors
    CSpaceCalls::create<CSpaceBase>(
        cap, &holder, CapIdx(space_idx, slot2 + 1),
        PermissionBits(1, PermissionBits::Type::NONE), sp_opt.value());
}

void kernel_setup(void) {
    pre_init();
    while (true);
}

void buddy_test_complex() {
    LOGGER::INFO("========== Complex Buddy Allocator Test Start ==========");

    // Scenario 1: Mixed Size Allocation & Fragmentation
    LOGGER::INFO("Scenario 1: Mixed Size & Fragmentation");
    PhyAddr p1 = GFP::get_free_page(1);  // 1 page
    PhyAddr p2 = GFP::get_free_page(2);  // 2 pages
    PhyAddr p4 = GFP::get_free_page(4);  // 4 pages
    PhyAddr p3 = GFP::get_free_page(3);  // 3 pages (odd size)

    if (p1.nonnull() && p2.nonnull() && p3.nonnull() && p4.nonnull()) {
        LOGGER::INFO("Mixed Alloc Success: 1p@%p, 2p@%p, 4p@%p, 3p@%p",
                     p1.addr(), p2.addr(), p4.addr(), p3.addr());

        // Free middle blocks to create fragmentation
        GFP::put_page(p2, 2);
        GFP::put_page(p3, 3);
        LOGGER::INFO("Freed middle blocks (2p, 3p)");

        // Try to alloc 2 pages again (should reuse p2 or part of p3)
        PhyAddr p_new = GFP::get_free_page(2);
        LOGGER::INFO("Re-alloc 2 pages: %p", p_new.addr());

        if (p_new.nonnull())
            GFP::put_page(p_new, 2);
        GFP::put_page(p1, 1);
        GFP::put_page(p4, 4);
    } else {
        LOGGER::ERROR("Mixed Alloc Failed");
        if (p1.nonnull())
            GFP::put_page(p1, 1);
        if (p2.nonnull())
            GFP::put_page(p2, 2);
        if (p4.nonnull())
            GFP::put_page(p4, 4);
        if (p3.nonnull())
            GFP::put_page(p3, 3);
    }

    // Scenario 2: Exhaustion Test (Specific Order)
    LOGGER::INFO("Scenario 2: Try to exhaust order 0 (1 page)");
    constexpr int MAX_singles = 32;
    PhyAddr singles[MAX_singles];
    int alloc_count = 0;

    for (int i = 0; i < MAX_singles; i++) {
        singles[i] = GFP::get_free_page(1);
        if (!singles[i].nonnull()) {
            LOGGER::WARN("Stopped at %d allocations", i);
            break;
        }
        alloc_count++;
    }
    LOGGER::INFO("Allocated %d single pages", alloc_count);

    // Reverse free to encourage merging
    for (int i = alloc_count - 1; i >= 0; i--) {
        GFP::put_page(singles[i], 1);
    }
    LOGGER::INFO("Freed all single pages");

    // Scenario 3: Large Block Split & Merge
    LOGGER::INFO("Scenario 3: Large Block Split & Merge");
    PhyAddr huge = GFP::get_free_page(64);  // Request 64 pages
    if (huge.nonnull()) {
        LOGGER::INFO("Huge block allocated at %p", huge);
        GFP::put_page(huge, 64);
        LOGGER::INFO("Huge block freed");

        // Verify we can alloc it again (merge successful)
        PhyAddr huge2 = GFP::get_free_page(64);
        if (huge2 == huge) {
            LOGGER::INFO(
                "Merge Verification Success: Same address re-allocated");
        } else {
            LOGGER::WARN(
                "Merge Verification: Got different address %p (Original: %p)",
                huge2, huge);
        }
        if (huge2.nonnull())
            GFP::put_page(huge2, 64);
    } else {
        LOGGER::ERROR("Huge block alloc failed");
    }

    LOGGER::INFO("========== Complex Buddy Allocator Test End ==========");
}

void slub_test_basic() {
    struct SlubSmallObj {
        uint32_t a;
        uint64_t b;
        char pad[24];
    };

    struct SlubHugeObj {
        char data[3000];
    };
    LOGGER::INFO("========== SLUB Test Start ==========");

    slub::SlubAllocator<SlubSmallObj> alloc;
    constexpr int kSmallCount             = 16;
    SlubSmallObj *small_objs[kSmallCount] = {nullptr};
    int small_alloc_ok                    = 0;

    LOGGER::INFO("SLUB Scenario A: small object alloc/free");
    for (int i = 0; i < kSmallCount; i++) {
        small_objs[i] = reinterpret_cast<SlubSmallObj *>(alloc.alloc());
        if (!small_objs[i]) {
            LOGGER::WARN("small alloc failed at idx=%d", i);
            continue;
        }
        small_objs[i]->a = static_cast<uint32_t>(0x1000 + i);
        small_objs[i]->b = static_cast<uint64_t>(0x20000000ull + i);
        small_alloc_ok++;
    }
    LOGGER::INFO("small alloc success count=%d/%d", small_alloc_ok,
                 kSmallCount);

    for (int i = 0; i < kSmallCount; i++) {
        if (small_objs[i]) {
            alloc.free(small_objs[i]);
            small_objs[i] = nullptr;
        }
    }

    auto small_stats = alloc.get_stats();
    LOGGER::INFO(
        "small stats: slabs=%lu inuse=%lu total_objs=%lu mem_bytes=%lu",
        static_cast<unsigned long>(small_stats.total_slabs),
        static_cast<unsigned long>(small_stats.objects_inuse),
        static_cast<unsigned long>(small_stats.objects_total),
        static_cast<unsigned long>(small_stats.memory_usage_bytes));

    LOGGER::INFO("SLUB Scenario B: small object reuse trend");
    SlubSmallObj *p1 = alloc.alloc();
    if (!p1) {
        LOGGER::ERROR("reuse test first alloc failed");
    } else {
        alloc.free(p1);
    }
    SlubSmallObj *p2 = alloc.alloc();
    LOGGER::INFO("reuse observe: p1=%p p2=%p", p1, p2);
    if (p2) {
        alloc.free(p2);
    }

    LOGGER::INFO("SLUB Scenario C: huge object path");
    slub::SlubAllocator<SlubHugeObj> huge_alloc;
    SlubHugeObj *h1 = huge_alloc.alloc();
    SlubHugeObj *h2 = huge_alloc.alloc();
    LOGGER::INFO("huge alloc: h1=%p h2=%p", h1, h2);
    if (!h1 || !h2) {
        LOGGER::WARN("huge alloc has failure: h1=%p h2=%p", h1, h2);
    }
    if (h1) {
        huge_alloc.free(h1);
    }
    if (h2) {
        huge_alloc.free(h2);
    }
    LOGGER::INFO("Test free nullptr");
    huge_alloc.free(nullptr);

    auto huge_stats = huge_alloc.get_stats();
    LOGGER::INFO("huge stats: slabs=%lu inuse=%lu total_objs=%lu mem_bytes=%lu",
                 static_cast<unsigned long>(huge_stats.total_slabs),
                 static_cast<unsigned long>(huge_stats.objects_inuse),
                 static_cast<unsigned long>(huge_stats.objects_total),
                 static_cast<unsigned long>(huge_stats.memory_usage_bytes));

    LOGGER::INFO("========== SLUB Test End ==========");
}

constexpr size_t TREE_SIZE = 8192;
struct TestTree {
    util::TreeNode<TestTree, util::tree_lca_tag> tree_node;
    int idx;
} NODES[TREE_SIZE];

void tree_test(void) {
    for (size_t i = 0; i < TREE_SIZE; i++) {
        NODES[i].idx = i;
    }

    using Tree = util::Tree<TestTree, &TestTree::tree_node, util::tree_lca_tag>;
    Tree::link_child(NODES[0], NODES[1]);
    Tree::link_child(NODES[0], NODES[2]);
    Tree::link_child(NODES[0], NODES[3]);
    Tree::link_child(NODES[0], NODES[4]);

    Tree::link_child(NODES[1], NODES[5]);
    Tree::link_child(NODES[1], NODES[6]);
    Tree::link_child(NODES[1], NODES[7]);

    Tree::link_child(NODES[2], NODES[8]);
    Tree::link_child(NODES[2], NODES[9]);

    Tree::link_child(NODES[3], NODES[10]);

    Tree::link_child(NODES[4], NODES[11]);

    Tree::link_child(NODES[5], NODES[12]);
    Tree::link_child(NODES[5], NODES[13]);

    Tree::link_child(NODES[3], NODES[14]);

    Tree::link_child(NODES[13], NODES[15]);

    for (int i = 0; i < 16; i++) {
        if (Tree::is_root(NODES[i])) {
            int cs = Tree::get_children(NODES[i]).size();
            kprintf("节点 %d 为根, 拥有 %d 个子节点\n", i, cs);
        } else {
            int p  = Tree::get_parent(NODES[i]).idx;
            int cs = Tree::get_children(NODES[i]).size();
            kprintf("节点 %d 的父亲为节点 %d, 拥有 %d 个子节点\n", i, p, cs);
        }
    }

    Tree::foreach_pre(NODES[0],
                      [](TestTree &node) { kprintf("%d ", node.idx); });

    kprintf("\n");

    auto print_lca = [=](size_t a, size_t b) {
        kprintf("%d 与 %d 的 LCA 为 %d \n", a, b,
                Tree::lca(&NODES[a], &NODES[b])->idx);
    };

    print_lca(0, 1);
    print_lca(0, 2);

    print_lca(1, 2);
    print_lca(5, 7);
    print_lca(4, 8);
    print_lca(10, 14);
    print_lca(11, 13);
    print_lca(12, 15);

    print_lca(0, 0);
    print_lca(12, 12);
}

void fs_test(void) {
    VFS vfs;
    // Register Tarfs
    vfs.register_fs(new tarfs::TarFSDriver());

    RamDiskDevice *initrd = make_initrd();
    FSErrCode code =
        vfs.mount("tarfs", initrd, "/initrd", MountFlags::NONE, "");
    if (code != FSErrCode::SUCCESS) {
        LOGGER::ERROR("vfs.mount()失败, errcode=%s", to_string(code));
        return;
    }

    FSOptional<VFile *> file_opt = vfs._open("/initrd/kernel/main.cpp", 0);
    if (!file_opt.present()) {
        LOGGER::ERROR("vfs._open()失败, errcode=%s",
                      to_string(file_opt.error()));
        return;
    }
    VFile *file = file_opt.value();

    char *source_code = new char[10 * PAGESIZE];
    vfs._read(file, source_code, 10 * PAGESIZE);
    kputs(source_code);
    vfs._close(file);

    code = vfs.umount("/initrd");
    if (code != FSErrCode::SUCCESS) {
        LOGGER::ERROR("vfs.umount()失败, errcode=%s", to_string(code));
        return;
    }
    delete[] source_code;
}