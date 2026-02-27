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
#include <cap/capdef.h>
#include <object/csa.h>
#include <object/testobj.h>
#include <perm/permission.h>
#include <perm/testobj.h>
#include <perm/csa.h>
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
void capability_test(void);
void tree_test(void);
void tree_base_test(void);
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

    // buddy_test_complex();
    // slub_test_basic();
    capability_test();
    // fs_test();
    // tree_test();
    // tree_base_test();

    // for (size_t i = 0; i < 10; i++) {
    //     TCB *t = new TCB();
    //     PCB *p = new PCB();
    //     kprintf("%p;%p\n", t, p);
    // }

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
    util::tree::TreeNode<TestTree, util::tree::tree_lca_tag> tree_node;
    int idx;
} NODES[TREE_SIZE];

void tree_test(void) {
    for (size_t i = 0; i < TREE_SIZE; i++) {
        NODES[i].idx = i;
    }

    using Tree = util::tree::Tree<TestTree, &TestTree::tree_node,
                                  util::tree::tree_lca_tag>;
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

using TreeA = util::tree_base::TreeNode<class Data, class TreeATag>;
using TreeB = util::tree_base::TreeLCANode<class Data, class TreeBTag>;

class Data : public TreeA, public TreeB {
public:
    int data;
} pool[TREE_SIZE];

void tree_base_test(void) {
    for (int i = 0; i < TREE_SIZE; i++) {
        pool[i].data = i;
    }
    auto &rt = pool[0];
    rt.TreeA::link_child(pool[1]);
    pool[1].TreeA::link_child(pool[2]);
    pool[1].TreeA::link_child(pool[3]);
    pool[3].TreeA::link_child(pool[4]);
    rt.TreeB::link_child(pool[5]);
    pool[5].TreeB::link_child(pool[6]);
    pool[6].TreeB::link_child(pool[7]);
    pool[6].TreeB::link_child(pool[8]);
    pool[5].TreeB::link_child(pool[9]);
    rt.TreeA::foreach_pre(
        [](const Data &p) { kprintf("TreeA node: %d\n", p.data); });
    rt.TreeB::foreach_pre(
        [](const Data &p) { kprintf("TreeB node: %d\n", p.data); });

    auto print_lca = [=](size_t a, size_t b) {
        kprintf("%d 与 %d 的 LCA 为 %d \n", a, b,
                TreeB::lca(&pool[a], &pool[b])->data);
    };

    print_lca(7, 9);
    print_lca(7, 8);
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

void capability_test(void) {
    LOGGER::INFO("开始能力系统测试...");

    // 1. 初始化 CUniverse 和 CSpace
    CUniverse universe;
    CSpace *space0 = universe.new_space();
    CSpace *space1 = universe.new_space();
    if (space0 == nullptr || space1 == nullptr) {
        LOGGER::ERROR("测试失败: CSpace 创建失败");
        return;
    }

    CapIdx idx0(0, 0); // 槽位 (0,0)
    CapIdx idx1(0, 1); // 槽位 (0,1)
    CapIdx idx2(0, 2); // 槽位 (0,2)
    CapIdx idx3(0, 3); // 槽位 (0,3)

    // 2. 手动构造第一个 CSA (space0 的访问器, 存放在 space0:idx0)
    CapErrCode err = space0->create<CSpaceAccessor>(idx0, space0);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 手动创建首个 CSA 失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("手动创建首个 CSA (指向 space0) 成功");

    // 获取该能力对象
    auto cap0_opt = space0->get(idx0);
    if (!cap0_opt.present()) {
        LOGGER::ERROR("测试失败: 获取 CSA0 失败");
        return;
    }
    Capability *cap_csaall = cap0_opt.value();
    CSAOperation csa_all(cap_csaall);

    // 3. 使用 CSA0 创建 TestObject (存放在 space0:idx1)
    err = csa_all.create<TestObject>(idx1, 12345);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 通过 CSA0 创建 TestObject 失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("通过 CSA0 在 space0:idx1 创建 TestObject(12345) 成功");

    // 验证 TestObject 的值
    auto cap_obj1_opt = space0->get(idx1);
    if (!cap_obj1_opt.present()) {
        LOGGER::ERROR("测试失败: 获取 TestObject1 失败");
        return;
    }
    TestObjectOperation op_obj1(cap_obj1_opt.value());
    auto val_opt = op_obj1.read();
    if (!val_opt.present()) {
        LOGGER::ERROR("测试失败: 读取 TestObject1 值失败");
        return;
    }
    LOGGER::INFO("读取 TestObject 值: %d", val_opt.value());
    if (val_opt.value() != 12345) {
        LOGGER::ERROR("测试失败: TestObject1 值验证失败, 期望 12345, 实际 %d", val_opt.value());
        return;
    }

    // 4. 使用 CSA0 为 space1 创建 CSA (指向 space1, 存放在 space0:idx2)
    err = csa_all.create<CSpaceAccessor>(idx2, space1);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 通过 CSA0 创建 CSA1 失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("通过 CSA0 在 space0:idx2 创建指向 space1 的 CSA 成功");

    // 获取 CSA1 并进行操作
    auto cap_csa1_opt = space0->get(idx2);
    if (!cap_csa1_opt.present()) {
        LOGGER::ERROR("测试失败: 获取 CSA1 失败");
        return;
    }
    CSAOperation op_csa1(cap_csa1_opt.value());

    // 5. 使用 CSA1 在 space1:idx0 创建 TestObject (值为 54321)
    err = op_csa1.create<TestObject>(idx0, 54321);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 通过 CSA1 创建 TestObject 失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("通过 CSA1 在 space1:idx0 创建 TestObject(54321) 成功");

    // 6. 使用 CSA0 将 space0:idx1 的 TestObject 克隆到 space0:idx3
    err = csa_all.clone(idx3, space0, idx1);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 通过 CSA0 克隆对象失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("通过 CSA0 将 space0:idx1 克隆到 space0:idx3 成功");

    // 验证克隆出来的对象
    auto cap_obj3_opt = space0->get(idx3);
    if (!cap_obj3_opt.present()) {
        LOGGER::ERROR("测试失败: 获取克隆对象失败");
        return;
    }
    TestObjectOperation op_obj3(cap_obj3_opt.value());
    auto read_obj3_opt = op_obj3.read();
    if (!read_obj3_opt.present()) {
        LOGGER::ERROR("测试失败: 读取克隆对象值失败");
        return;
    }
    if (read_obj3_opt.value() != 12345) {
        LOGGER::ERROR("测试失败: 克隆对象值验证失败, 期望 12345, 实际 %d", read_obj3_opt.value());
        return;
    }
    LOGGER::INFO("验证克隆对象值成功: %d", read_obj3_opt.value());

    // 7. 使用 CSA1 将 space0:idx1 迁移到 space1:idx1
    err = op_csa1.migrate(idx1, space0, idx1);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 通过 CSA1 迁移对象失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("通过 CSA1 将 space0:idx1 迁移到 space1:idx1 成功");

    // 检查原位置是否为空
    if (space0->get(idx1).present()) {
        LOGGER::ERROR("测试失败: 验证原位置 (space0:idx1) 迁移后仍不为空");
        return;
    }
    LOGGER::INFO("验证原位置 (space0:idx1) 已为空");

    // 检查新位置
    auto cap_migrated_opt = space1->get(idx1);
    if (!cap_migrated_opt.present()) {
        LOGGER::ERROR("测试失败: 获取迁移后对象失败");
        return;
    }
    TestObjectOperation op_migrated(cap_migrated_opt.value());
    auto read_migrated_opt = op_migrated.read();
    if (!read_migrated_opt.present()) {
        LOGGER::ERROR("测试失败: 读取迁移后对象值失败");
        return;
    }
    if (read_migrated_opt.value() != 12345) {
        LOGGER::ERROR("测试失败: 迁移后对象值验证失败, 期望 12345, 实际 %d", read_migrated_opt.value());
        return;
    }
    LOGGER::INFO("验证迁移后对象 (space1:idx1) 值成功: %d", read_migrated_opt.value());

    // 8. 测试权限降级 (选用 TestObject 进行测试)
    PermissionBits low_perm(perm::testobj::READ, PayloadType::TEST_OBJECT);
    err = cap_migrated_opt.value()->downgrade(low_perm);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 权限降级操作失败, 错误码: %s", to_string(err));
        return;
    }
    LOGGER::INFO("对迁移后的 TestObject 进行权限降级 (仅保留 READ)");

    // 尝试写操作, 应该失败 (其 logic 内部会报错)
    op_migrated.increase();
    auto read_after_inc_opt = op_migrated.read();
    if (!read_after_inc_opt.present()) {
        LOGGER::ERROR("测试失败: 降级后读取失败");
        return;
    }
    if (read_after_inc_opt.value() != 12345) {
        LOGGER::ERROR("测试失败: 无权限的 increase 操作生效了, 值变为: %d", read_after_inc_opt.value());
        return;
    }
    LOGGER::INFO("验证无权限的 increase 操作未生效");

    // 再次降级, 去掉 READ
    PermissionBits none_perm(0, PayloadType::TEST_OBJECT);
    err = cap_migrated_opt.value()->downgrade(none_perm);
    if (err != CapErrCode::SUCCESS) {
        LOGGER::ERROR("测试失败: 权限降级(none)操作失败, 错误码: %s", to_string(err));
        return;
    }
    auto final_read_opt = op_migrated.read();
    if (final_read_opt.present()) {
        LOGGER::ERROR("测试失败: 验证无权限的 read 操作竟然成功了");
        return;
    }
    if (final_read_opt.error() != CapErrCode::INSUFFICIENT_PERMISSIONS) {
        LOGGER::ERROR("测试失败: 无权限的 read 操作返回了错误的错误码: %s", to_string(final_read_opt.error()));
        return;
    }
    LOGGER::INFO("验证无权限的 read 操作失败, 并返回错误码: %s",
                 to_string(final_read_opt.error()));

    // 9. 复杂派生树与 Revoke 测试 (增加宽度与跨 Space 深度)
    LOGGER::INFO("开始复杂派生树 (宽度 6->1, 跨 Space 深度 4) 与 Revoke 测试...");
    CapIdx idx_root(0, 10);
    err = csa_all.create<TestObject>(idx_root, 999);
    if (err != CapErrCode::SUCCESS) return;
    auto cap_root = space0->get(idx_root).value();

    // 构建测试矩阵: L1 (6个) -> L2 (5个) -> L3 (4个) -> L4 (1个)
    // 其中 L3 存放在 space1 中实现跨 Space 验证
    
    // L1: 6个子节点在 space0 (idx 11-16)
    for (int i = 0; i < 6; ++i) {
        err = csa_all.clone(CapIdx(0, 11 + i), space0, idx_root);
        if (err != CapErrCode::SUCCESS) return;
    }
    auto cap_L1_base = space0->get(CapIdx(0, 11)).value(); // 我们将以第一个 L1 为父节点继续派生
    LOGGER::INFO("L1 层构建成功: 6 个子节点 (space0:idx11-16)");

    // L2: 从 space0:idx11 派生 5个子节点在 space0 (idx 17-21)
    for (int i = 0; i < 5; ++i) {
        err = csa_all.clone(CapIdx(0, 17 + i), space0, CapIdx(0, 11));
        if (err != CapErrCode::SUCCESS) return;
    }
    auto cap_L2_base = space0->get(CapIdx(0, 17)).value();
    LOGGER::INFO("L2 层构建成功: 5 个子节点 (space0:idx17-21, 父节点 idx11)");

    // L3: 从 space0:idx17 跨 Space 派生 4个子节点到 space1 (idx 10-13)
    for (int i = 0; i < 4; ++i) {
        err = op_csa1.clone(CapIdx(0, 10 + i), space0, CapIdx(0, 17));
        if (err != CapErrCode::SUCCESS) return;
    }
    auto cap_L3_base = space1->get(CapIdx(0, 10)).value();
    LOGGER::INFO("L3 层构建成功: 4 个跨 Space 子节点 (space1:idx10-13, 父节点 space0:idx17)");

    // L4: 从 space1:idx10 派生 1个子节点在 space1 (idx 14)
    err = op_csa1.clone(CapIdx(0, 14), space1, CapIdx(0, 10));
    if (err != CapErrCode::SUCCESS) return;
    LOGGER::INFO("L4 层构建成功: 1 个最终节点 (space1:idx14)");

    // 执行核心 Revoke 操作: 删除 L1 中的第一个节点 (idx 11)
    // 预期: 它的所有后代 (L2中的5个, L3中的4个, L4中的1个) 必须被全部自动清理
    LOGGER::INFO("执行 Revoke 操作: 删除 L1 根节点 (space0:idx11)...");
    err = csa_all.remove(CapIdx(0, 11));
    if (err != CapErrCode::SUCCESS) return;

    // 验证联动删除
    bool revoke_failed = false;
    LOGGER::INFO("验证 Revoke 结果: idx11 及其所有后代应该都被删除...");
    LOGGER::INFO("检查 L2, 如果成功应报错: 槽位索引未被占用");
    // 检查 L2
    for (int i = 0; i < 5; ++i) {
        if (space0->get(CapIdx(0, 17 + i)).present()) revoke_failed = true;
    }
    if (revoke_failed) {
        LOGGER::ERROR("Revoke 验证失败: 派生树中部分子节点未被正确清理");
        return;
    }
    LOGGER::INFO("检查 L3, 如果成功应报错: 槽位索引未被占用");
    // 检查 L3 (跨 Space)
    for (int i = 0; i < 4; ++i) {
        if (space1->get(CapIdx(0, 10 + i)).present()) revoke_failed = true;
    }
    if (revoke_failed) {
        LOGGER::ERROR("Revoke 验证失败: 派生树中部分子节点未被正确清理");
        return;
    }
    LOGGER::INFO("检查 L4, 如果成功应报错: 槽位索引未被占用");
    // 检查 L4
    if (space1->get(CapIdx(0, 14)).present()) revoke_failed = true;

    if (revoke_failed) {
        LOGGER::ERROR("Revoke 验证失败: 派生树中部分子节点未被正确清理");
        return;
    }

    // 验证兄弟节点 (L1 的其它 5 个节点) 应该还活着
    LOGGER::INFO("检查 L1 的其它5个节点是否依然存活");
    for (int i = 1; i < 6; ++i) {
        if (!space0->get(CapIdx(0, 11 + i)).present()) {
            LOGGER::ERROR("Revoke 错误: 意外删除了兄弟节点 idx%d", 11 + i);
            return;
        }
    }
    LOGGER::INFO("验证兄弟节点存活成功");

    LOGGER::INFO("检查根节点是否依然存活");
    // 验证根节点
    if (!space0->get(idx_root).present()) {
        LOGGER::ERROR("Revoke 错误: 根节点被删除");
        return;
    }
    LOGGER::INFO("验证根节点存活成功");

    LOGGER::INFO("复杂宽度/深度派生树 Revoke 验证全部成功!");
    LOGGER::INFO("所有能力系统测试已圆满完成!");
}