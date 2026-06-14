/**
 * @file buddy.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * @brief Buddy页框分配器实现
 * @version alpha-1.0.0
 * @date 2026-01-29
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <arch/trait.h>
#include <boot/sbi/sbi_paging.h>
#include <env.h>
#include <logger.h>
#include <mem/buddy.h>
#include <sus/logger.h>
#include <sus/range.h>
#include <symbols.h>

#include <cstddef>
#include <cstring>

namespace {
    constexpr size_t FREEBLOCK_BITS_PER_WORD = sizeof(size_t) * 8;
    constexpr addr_t SBI_RECLAIM_BEGIN       = 0x0000000080200000ULL;
    constexpr addr_t SBI_RECLAIM_END         = 0x0000000080230000ULL;

    [[nodiscard]]
    bool test_bitmap_bit(const size_t *bitmap, size_t idx) noexcept {
        return (bitmap[idx / FREEBLOCK_BITS_PER_WORD] &
                (1ul << (idx % FREEBLOCK_BITS_PER_WORD))) != 0;
    }

    void set_bitmap_bit(size_t *bitmap, size_t idx) noexcept {
        bitmap[idx / FREEBLOCK_BITS_PER_WORD] |=
            1ul << (idx % FREEBLOCK_BITS_PER_WORD);
    }

    void clear_bitmap_bit(size_t *bitmap, size_t idx) noexcept {
        bitmap[idx / FREEBLOCK_BITS_PER_WORD] &=
            ~(1ul << (idx % FREEBLOCK_BITS_PER_WORD));
    }
}  // namespace

PhyArea BuddyAllocator::pool0_area() noexcept {
    PhyAddr begin = convert_pointer(_buddy_pool0);
    return PhyArea(begin, begin + sizeof(_buddy_pool0));
}

BuddyAllocator::FreeBlockPool *BuddyAllocator::pool0_head() noexcept {
    return reinterpret_cast<FreeBlockPool *>(_buddy_pool0);
}

template <KernelStage Stage>
BuddyAllocator::FreeBlockPool *BuddyAllocator::runtime_pool(
    PhyAddr paddr) noexcept {
    if (!paddr.nonnull()) {
        return nullptr;
    }

    if (util::range::within(pool0_area(), paddr)) {
        size_t offset = static_cast<size_t>(paddr - pool0_area().begin);
        return reinterpret_cast<FreeBlockPool *>(
            reinterpret_cast<byte *>(_buddy_pool0) + offset);
    }

    if constexpr (Stage == KernelStage::PRE_INIT) {
        return paddr.as<FreeBlockPool>();
    } else {
        return convert<KpaAddr>(paddr).as<FreeBlockPool>();
    }
}

template <KernelStage Stage>
BuddyAllocator::FreeBlock *BuddyAllocator::runtime_block(
    PhyAddr paddr) noexcept {
    if (!paddr.nonnull()) {
        return nullptr;
    }

    if (util::range::within(pool0_area(), paddr)) {
        size_t offset = static_cast<size_t>(paddr - pool0_area().begin);
        return reinterpret_cast<FreeBlock *>(
            reinterpret_cast<byte *>(_buddy_pool0) + offset);
    }

    if constexpr (Stage == KernelStage::PRE_INIT) {
        return paddr.as<FreeBlock>();
    } else {
        return convert<KpaAddr>(paddr).as<FreeBlock>();
    }
}

PhyAddr BuddyAllocator::pool_to_pa(const FreeBlockPool *pool) noexcept {
    return convert_pointer(const_cast<FreeBlockPool *>(pool));
}

BuddyAllocator::FreeBlock *BuddyAllocator::pool_to_blocks(
    const FreeBlockPool *pool) noexcept {
    return reinterpret_cast<FreeBlock *>(const_cast<FreeBlockPool *>(pool));
}

template <KernelStage Stage>
void BuddyAllocator::init_pool(FreeBlockPool *pool,
                               PhyAddr pool_paddr) noexcept {
    memset(pool, 0, FREEBLOCK_HEADER_BYTES);
    pool->used = FREEBLOCK_HEADER_BLOCKS;
    for (size_t i = 0; i < FREEBLOCK_HEADER_BLOCKS; ++i) {
        set_bitmap_bit(pool->bitmap, i);
    }

    auto *blocks = runtime_block<Stage>(pool_paddr);
    for (size_t i = FREEBLOCK_HEADER_BLOCKS; i < FREEBLOCK_POOL_SIZE; ++i) {
        blocks[i].paddr = pool_paddr + i * sizeof(FreeBlock);
        blocks[i].order = 0;
        blocks[i].prev  = nullptr;
        blocks[i].next  = nullptr;
    }
}

void BuddyAllocator::init_pool0() noexcept {
    init_pool<KernelStage::PRE_INIT>(pool0_head(), pool0_area().begin);
    _pool_head = pool0_head();
    _pool_tail = pool0_head();
}

void BuddyAllocator::attach_pool(FreeBlockPool *pool) noexcept {
    pool->next = nullptr;
    pool->prev = _pool_tail;
    if (_pool_tail != nullptr) {
        _pool_tail->next = pool;
    } else {
        _pool_head = pool;
    }
    _pool_tail = pool;
}

bool BuddyAllocator::pool_needs_expand(const FreeBlockPool *pool) noexcept {
    return pool->used * 4 > FREEBLOCK_POOL_SIZE * 3;
}

template <KernelStage Stage>
Result<void> BuddyAllocator::maybe_expand_pool() noexcept {
    if (_pool_tail == nullptr || !pool_needs_expand(_pool_tail)) {
        void_return();
    }

    loggers::BUDDY::DEBUG("FreeBlocks 池开始扩容");

    // 超过阈值时至少还有 (128 - 24) 个块可用, 因此扩容过程中不必担心块不够。
    return add_new_pool<Stage>();
}

template <KernelStage Stage>
Result<void> BuddyAllocator::add_new_pool() noexcept {
    auto pool_res = fetch_frame_order<Stage>(
        pages2order(FREEBLOCK_EXPAND_PAGES));
    if (!pool_res.has_value()) {
        propagate_return(pool_res);
    }

    PhyAddr pool_paddr = pool_res.value();
    auto *pool         = runtime_pool<Stage>(pool_paddr);
    init_pool<Stage>(pool, pool_paddr);
    attach_pool(pool);
    loggers::BUDDY::INFO("追加 FreeBlock 池: [%p, %p)", pool_paddr.addr(),
                         (pool_paddr + FREEBLOCK_EXPAND_PAGES * PAGESIZE).addr());
    void_return();
}

template <KernelStage Stage>
Result<BuddyAllocator::FreeBlock *> BuddyAllocator::alloc_freeblock() noexcept {
    for (FreeBlockPool *pool = _pool_head; pool != nullptr; pool = pool->next) {
        if (pool->used >= FREEBLOCK_POOL_SIZE) {
            continue;
        }

        for (size_t idx = FREEBLOCK_HEADER_BLOCKS; idx < FREEBLOCK_POOL_SIZE; ++idx) {
            if (test_bitmap_bit(pool->bitmap, idx)) {
                continue;
            }

            set_bitmap_bit(pool->bitmap, idx);
            ++pool->used;
            FreeBlock *block = &pool_to_blocks(pool)[idx];
            block->prev      = nullptr;
            block->next      = nullptr;
            return block;
        }
    }

    auto expand_res = add_new_pool<Stage>();
    if (!expand_res.has_value()) {
        unexpect_return(expand_res.error());
    }

    for (size_t idx = FREEBLOCK_HEADER_BLOCKS; idx < FREEBLOCK_POOL_SIZE; ++idx) {
        if (test_bitmap_bit(_pool_tail->bitmap, idx)) {
            continue;
        }
        set_bitmap_bit(_pool_tail->bitmap, idx);
        ++_pool_tail->used;
        FreeBlock *block = &pool_to_blocks(_pool_tail)[idx];
        block->prev      = nullptr;
        block->next      = nullptr;
        return block;
    }

    unexpect_return(ErrCode::OUT_OF_MEMORY);
}

void BuddyAllocator::free_freeblock(FreeBlock *block) noexcept {
    if (block == nullptr) {
        return;
    }

    PhyAddr block_pa = convert_pointer(block);
    for (FreeBlockPool *pool = _pool_head; pool != nullptr; pool = pool->next) {
        PhyAddr pool_pa      = pool_to_pa(pool);
        PhyAddr pool_end_pa  = pool_pa + FREEBLOCK_POOL_SIZE * sizeof(FreeBlock);
        if (block_pa < pool_pa || block_pa >= pool_end_pa) {
            continue;
        }

        size_t idx = static_cast<size_t>((block_pa - pool_pa) / sizeof(FreeBlock));
        assert(idx >= FREEBLOCK_HEADER_BLOCKS);
        assert(test_bitmap_bit(pool->bitmap, idx));

        clear_bitmap_bit(pool->bitmap, idx);
        --pool->used;
        block->prev = nullptr;
        block->next = nullptr;
        return;
    }

    panic("BuddyAllocator::free_freeblock 无法找到所属池");
}

void BuddyAllocator::link_block(FreeBlock *node) noexcept {
    FreeBlock *&head = free_area[node->order];
    if (head == nullptr) {
        head = node;
        return;
    }

    if (node->paddr < head->paddr) {
        node->next = head;
        head->prev = node;
        head       = node;
        return;
    }

    FreeBlock *iter = head;
    while (iter->next != nullptr && iter->next->paddr < node->paddr) {
        iter = iter->next;
    }

    node->next = iter->next;
    node->prev = iter;
    if (iter->next != nullptr) {
        iter->next->prev = node;
    }
    iter->next = node;
}

void BuddyAllocator::unlink_block(FreeBlock *node) noexcept {
    if (node->prev != nullptr) {
        node->prev->next = node->next;
    } else {
        free_area[node->order] = node->next;
    }
    if (node->next != nullptr) {
        node->next->prev = node->prev;
    }

    node->prev = nullptr;
    node->next = nullptr;
}

BuddyAllocator::FreeBlock *BuddyAllocator::find_buddy_node(
    FreeBlock *node) noexcept {
    size_t order = node->order;
    if (order > MAX_BUDDY_ORDER) {
        return nullptr;
    }

    size_t size         = block_size_for_order(order);
    PhyAddr buddy_paddr = PhyAddr(node->paddr.arith() ^ size);

    if (node->prev != nullptr && node->prev->paddr == buddy_paddr &&
        node->prev->order == order)
    {
        return node->prev;
    }
    if (node->next != nullptr && node->next->paddr == buddy_paddr &&
        node->next->order == order)
    {
        return node->next;
    }
    return nullptr;
}

void BuddyAllocator::pre_init() {
    for (int order = 0; order <= MAX_BUDDY_ORDER; ++order) {
        free_area[order] = nullptr;
    }
    _post_initialized = false;
    init_pool0();

    auto &meminfo = env::inst().meminfo();
    for (size_t i = 0; i < meminfo.region_cnt; ++i) {
        const MemRegion &region = meminfo.regions[i];
        if (region.status != MemRegion::MemoryStatus::FREE) {
            continue;
        }

        PhyAddr start_addr = region.ptr.page_align_up();
        PhyAddr end_addr   = (region.ptr + region.size).page_align_down();
        size_t pages       = (end_addr - start_addr) / PAGESIZE;
        if (pages == 0) {
            continue;
        }

        loggers::BUDDY::DEBUG("添加可用内存区域 [%p, %p), 共 %u 页",
                              start_addr.addr(), end_addr.addr(),
                              static_cast<unsigned>(pages));
        add_memory_range<KernelStage::PRE_INIT>(start_addr, pages);
    }

    PhyAddr sbi_begin = PhyAddr(SBI_RECLAIM_BEGIN).page_align_up();
    PhyAddr sbi_end   = PhyAddr(SBI_RECLAIM_END).page_align_down();
    if (sbi_begin < sbi_end) {
        size_t pages = (sbi_end - sbi_begin) / PAGESIZE;
        loggers::BUDDY::INFO("回收 SBI 引导区 [%p, %p), 共 %u 页",
                             sbi_begin.addr(), sbi_end.addr(),
                             static_cast<unsigned>(pages));
        add_memory_range<KernelStage::PRE_INIT>(sbi_begin, pages);
    }
}

void BuddyAllocator::post_init() {
    if (_post_initialized) {
        return;
    }

    for (FreeBlockPool *pool = _pool_head; pool != nullptr;) {
        PhyAddr pool_pa         = pool_to_pa(pool);
        PhyAddr next_pa         = pool->next == nullptr
                                      ? PhyAddr::null
                                      : convert_pointer(pool->next);
        PhyAddr prev_pa         = pool->prev == nullptr
                                      ? PhyAddr::null
                                      : convert_pointer(pool->prev);
        FreeBlockPool *next_raw = pool->next;

        FreeBlockPool *pool_new = runtime_pool<KernelStage::POST_INIT>(pool_pa);
        pool_new->next = runtime_pool<KernelStage::POST_INIT>(next_pa);
        pool_new->prev = runtime_pool<KernelStage::POST_INIT>(prev_pa);

        for (size_t idx = FREEBLOCK_HEADER_BLOCKS; idx < FREEBLOCK_POOL_SIZE; ++idx) {
            if (!test_bitmap_bit(pool_new->bitmap, idx)) {
                continue;
            }

            PhyAddr block_pa = pool_pa + idx * sizeof(FreeBlock);
            FreeBlock *block = runtime_block<KernelStage::POST_INIT>(block_pa);
            PhyAddr block_prev_pa = block->prev == nullptr
                                        ? PhyAddr::null
                                        : convert_pointer(block->prev);
            PhyAddr block_next_pa = block->next == nullptr
                                        ? PhyAddr::null
                                        : convert_pointer(block->next);

            block->prev = runtime_block<KernelStage::POST_INIT>(block_prev_pa);
            block->next = runtime_block<KernelStage::POST_INIT>(block_next_pa);
        }

        pool = next_raw;
    }

    for (int order = 0; order <= MAX_BUDDY_ORDER; ++order) {
        if (free_area[order] == nullptr) {
            continue;
        }
        free_area[order] = runtime_block<KernelStage::POST_INIT>(
            convert_pointer(free_area[order]));
    }

    if (_pool_head != nullptr) {
        _pool_head =
            runtime_pool<KernelStage::POST_INIT>(convert_pointer(_pool_head));
    }
    if (_pool_tail != nullptr) {
        _pool_tail =
            runtime_pool<KernelStage::POST_INIT>(convert_pointer(_pool_tail));
    }

    _post_initialized = true;
    __print_memory_layout<KernelStage::POST_INIT>();
    loggers::BUDDY::INFO("BuddyAllocator initialized with external FreeBlock pools.");
}

template BuddyAllocator::FreeBlockPool *
BuddyAllocator::runtime_pool<KernelStage::PRE_INIT>(PhyAddr) noexcept;
template BuddyAllocator::FreeBlockPool *
BuddyAllocator::runtime_pool<KernelStage::POST_INIT>(PhyAddr) noexcept;
template BuddyAllocator::FreeBlock *
BuddyAllocator::runtime_block<KernelStage::PRE_INIT>(PhyAddr) noexcept;
template BuddyAllocator::FreeBlock *
BuddyAllocator::runtime_block<KernelStage::POST_INIT>(PhyAddr) noexcept;
template void BuddyAllocator::init_pool<KernelStage::PRE_INIT>(
    FreeBlockPool *, PhyAddr) noexcept;
template void BuddyAllocator::init_pool<KernelStage::POST_INIT>(
    FreeBlockPool *, PhyAddr) noexcept;
template Result<void>
BuddyAllocator::maybe_expand_pool<KernelStage::PRE_INIT>() noexcept;
template Result<void>
BuddyAllocator::maybe_expand_pool<KernelStage::POST_INIT>() noexcept;
template Result<void>
BuddyAllocator::add_new_pool<KernelStage::PRE_INIT>() noexcept;
template Result<void>
BuddyAllocator::add_new_pool<KernelStage::POST_INIT>() noexcept;
template Result<BuddyAllocator::FreeBlock *>
BuddyAllocator::alloc_freeblock<KernelStage::PRE_INIT>() noexcept;
template Result<BuddyAllocator::FreeBlock *>
BuddyAllocator::alloc_freeblock<KernelStage::POST_INIT>() noexcept;
