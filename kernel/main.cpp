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

#include <arch/riscv64/mem/sv39.h>
#include <arch/trait.h>
#include <cap/capability.h>
#include <cap/capcall.h>
#include <cap/permission.h>
#include <configuration.h>
#include <kio.h>
#include <mem/alloc.h>
#include <mem/gfp.h>
#include <mem/kaddr.h>
#include <sus/baseio.h>
#include <sus/logger.h>
#include <sus/tree.h>
#include <sus/types.h>
#include <symbols.h>
#include <task/task.h>

#include <cstdarg>
#include <cstddef>
#include <cstring>

bool post_init_flag = false;

void buddy_test_complex();
void tree_test(void);

int kputs(const char *str) {
    if (!post_init_flag) {
        Serial::serial_write_string(str);
    } else {
        const char *_str = (const char *)KA2PA((void *)str);
        Serial::serial_write_string(_str);
    }
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

MemRegion regions[128];
PageMan::PTE *kernel_root = nullptr;
void *phymem_upper_bound;

void kernel_paging_setup() {
    // 创建内核页表管理器
    kernel_root = PageMan::make_root();
    PageMan kernelman(kernel_root);

    ker_paddr::init(phymem_upper_bound);
    ker_paddr::mapping_kernel_areas(kernelman);

    // 对[0, phymem_upper_bound)进行恒等映射
    size_t sz = (size_t)phymem_upper_bound;
    kernelman.map_range<true>((void *)0, (void *)0, sz,
                              PageMan::rwx(true, true, true), false, true);

    kernelman.switch_root();
    kernelman.flush_tlb();
}

void post_init(void) {
    // 将 sp 移动到高位内存
    RELOAD_SP();

    // 设置post_init标志
    post_init_flag = true;

    // logger
    LOGGER::INFO("已进入 post-init 阶段");

    // 将 pre-init 阶段中初始化的子系统再次初始化, 以适应内核虚拟地址空间
    GFP::post_init();
    PageMan::post_init();

    // 初始化默认 Allocator 子系统
    Allocator::init();

    // 初始化中断
    Interrupt::init();

    // 架构后初始化
    Initialization::post_init();

    // buddy_test_complex();
    tree_test();

    // 将低端内存设置为用户态
    PageMan kernelman(kernel_root);
    kernelman.modify_range_flags<PageMan::ModifyMask::U>(
        (void *)0, (size_t)phymem_upper_bound, PageMan::RWX::NONE, true, false);

    TCBManager::init();

    while (true);
}

void pre_init(void) {
    Initialization::pre_init();

    memset(regions, 0, sizeof(regions));
    int cnt           = MemLayout::detect_memory_layout(regions, 128);
    void *upper_bound = nullptr;
    for (int i = 0; i < cnt; i++) {
        LOGGER::INFO("探测到内存区域 %d: [%p, %p) Status: %d", i,
                     regions[i].ptr,
                     (void *)((umb_t)(regions[i].ptr) + regions[i].size),
                     static_cast<int>(regions[i].status));
        void *this_bound = (void *)((umb_t)(regions[i].ptr) + regions[i].size);
        if (upper_bound < this_bound) {
            upper_bound = this_bound;
        }
    }

    LOGGER::INFO("初始化线性增长GFP");
    GFP::pre_init(regions, cnt);

    LOGGER::INFO("初始化内核地址空间管理器");
    PageMan::pre_init();
    phymem_upper_bound = upper_bound;
    kernel_paging_setup();

    // 进入 post-init 阶段
    // 此阶段内, 内核的所有代码和数据均已映射到内核虚拟地址空间
    typedef void (*PostTestFuncType)(void);
    PostTestFuncType post_test_func =
        (PostTestFuncType)PA2KA((void *)post_init);
    LOGGER::DEBUG("跳转到内核虚拟地址空间中的post_init函数: %p",
                  post_test_func);
    post_test_func();
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
    CSpaceCalls::insert(cap, slot, cap);
    CSpaceCalls::remove<CSpaceBase>(cap, slot);
    CSpaceCalls::insert(cap, slot, cap);
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
    void *p1 = GFP::alloc_frame(1);  // 1 page
    void *p2 = GFP::alloc_frame(2);  // 2 pages
    void *p4 = GFP::alloc_frame(4);  // 4 pages
    void *p3 = GFP::alloc_frame(3);  // 3 pages (odd size)

    if (p1 && p2 && p3 && p4) {
        LOGGER::INFO("Mixed Alloc Success: 1p@%p, 2p@%p, 4p@%p, 3p@%p", p1, p2,
                     p4, p3);

        // Free middle blocks to create fragmentation
        GFP::free_frame(p2, 2);
        GFP::free_frame(p3, 3);
        LOGGER::INFO("Freed middle blocks (2p, 3p)");

        // Try to alloc 2 pages again (should reuse p2 or part of p3)
        void *p_new = GFP::alloc_frame(2);
        LOGGER::INFO("Re-alloc 2 pages: %p", p_new);

        if (p_new)
            GFP::free_frame(p_new, 2);
        GFP::free_frame(p1, 1);
        GFP::free_frame(p4, 4);
    } else {
        LOGGER::ERROR("Mixed Alloc Failed");
        if (p1)
            GFP::free_frame(p1, 1);
        if (p2)
            GFP::free_frame(p2, 2);
        if (p4)
            GFP::free_frame(p4, 4);
        if (p3)
            GFP::free_frame(p3, 3);
    }

    // Scenario 2: Exhaustion Test (Specific Order)
    LOGGER::INFO("Scenario 2: Try to exhaust order 0 (1 page)");
    constexpr int MAX_singles = 32;
    void *singles[MAX_singles];
    int alloc_count = 0;

    for (int i = 0; i < MAX_singles; i++) {
        singles[i] = GFP::alloc_frame(1);
        if (!singles[i]) {
            LOGGER::WARN("Stopped at %d allocations", i);
            break;
        }
        alloc_count++;
    }
    LOGGER::INFO("Allocated %d single pages", alloc_count);

    // Reverse free to encourage merging
    for (int i = alloc_count - 1; i >= 0; i--) {
        GFP::free_frame(singles[i], 1);
    }
    LOGGER::INFO("Freed all single pages");

    // Scenario 3: Large Block Split & Merge
    LOGGER::INFO("Scenario 3: Large Block Split & Merge");
    void *huge = GFP::alloc_frame(64);  // Request 64 pages
    if (huge) {
        LOGGER::INFO("Huge block allocated at %p", huge);
        GFP::free_frame(huge, 64);
        LOGGER::INFO("Huge block freed");

        // Verify we can alloc it again (merge successful)
        void *huge2 = GFP::alloc_frame(64);
        if (huge2 == huge) {
            LOGGER::INFO(
                "Merge Verification Success: Same address re-allocated");
        } else {
            LOGGER::WARN(
                "Merge Verification: Got different address %p (Original: %p)",
                huge2, huge);
        }
        if (huge2)
            GFP::free_frame(huge2, 64);
    } else {
        LOGGER::ERROR("Huge block alloc failed");
    }

    LOGGER::INFO("========== Complex Buddy Allocator Test End ==========");
}

constexpr size_t TREE_SIZE = 8192;
struct TestTree {
    util::TreeNode<TestTree, util::tree_lca_tag> tree_node;
    int idx;
} NODES[TREE_SIZE];

void tree_test(void) {
    for (size_t i = 0 ; i < TREE_SIZE ; i ++) {
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

    for (int i = 0 ; i < 16 ; i ++) {
        if (Tree::is_root(NODES[i])) {
            int cs = Tree::get_children(NODES[i]).size();
            kprintf("节点 %d 为根, 拥有 %d 个子节点\n", i, cs);
        }
        else {
            int p = Tree::get_parent(NODES[i]).idx;
            int cs = Tree::get_children(NODES[i]).size();
            kprintf("节点 %d 的父亲为节点 %d, 拥有 %d 个子节点\n", i, p ,cs);
        }
    }

    Tree::foreach_pre(NODES[0], [](TestTree &node) {
        kprintf("%d ", node.idx);
    });

    kprintf("\n");

    auto print_lca = [=](size_t a, size_t b) {
        kprintf ("%d 与 %d 的 LCA 为 %d \n", a, b, Tree::lca(&NODES[a], &NODES[b])->idx);
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