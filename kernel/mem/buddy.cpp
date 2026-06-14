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
#include <env.h>
#include <logger.h>
#include <mem/buddy.h>
#include <sus/logger.h>
#include <sus/range.h>

#include <cstddef>
#include <cstring>

namespace {
    constexpr size_t FREEBLOCK_BITS_PER_WORD = sizeof(size_t) * 8;

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

    return convert<KpaAddr>(paddr).as<FreeBlockPool>();
}

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

    return convert<KpaAddr>(paddr).as<FreeBlock>();
}

PhyAddr BuddyAllocator::pool_to_pa(const FreeBlockPool *pool) noexcept {
    return convert_pointer(const_cast<FreeBlockPool *>(pool));
}

BuddyAllocator::FreeBlock *BuddyAllocator::pool_to_blocks(
    const FreeBlockPool *pool) noexcept {
    return reinterpret_cast<FreeBlock *>(const_cast<FreeBlockPool *>(pool));
}

void BuddyAllocator::init_pool(FreeBlockPool *pool, PhyAddr pool_paddr) noexcept {
    memset(pool, 0, FREEBLOCK_HEADER_BYTES);
    pool->used = FREEBLOCK_HEADER_BLOCKS;
    for (size_t i = 0; i < FREEBLOCK_HEADER_BLOCKS; ++i) {
        set_bitmap_bit(pool->bitmap, i);
    }

    auto *blocks = runtime_block(pool_paddr);
    for (size_t i = FREEBLOCK_HEADER_BLOCKS; i < FREEBLOCK_POOL_SIZE; ++i) {
        blocks[i].paddr = pool_paddr + i * sizeof(FreeBlock);
        blocks[i].order = 0;
        blocks[i].prev  = nullptr;
        blocks[i].next  = nullptr;
    }
}

void BuddyAllocator::init_pool0() noexcept {
    init_pool(pool0_head(), pool0_area().begin);
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

Result<void> BuddyAllocator::maybe_expand_pool() noexcept {
    if (_pool_tail == nullptr || !pool_needs_expand(_pool_tail)) {
        void_return();
    }

    loggers::BUDDY::DEBUG("FreeBlocks 池开始扩容");

    // 超过阈值时至少还有 (128 - 24) 个块可用, 因此扩容过程中不必担心块不够。
    return add_new_pool();
}

Result<void> BuddyAllocator::add_new_pool() noexcept {
    auto pool_res = fetch_frame_order(pages2order(FREEBLOCK_EXPAND_PAGES));
    if (!pool_res.has_value()) {
        propagate_return(pool_res);
    }

    PhyAddr pool_paddr = pool_res.value();
    auto *pool         = runtime_pool(pool_paddr);
    init_pool(pool, pool_paddr);
    attach_pool(pool);
    loggers::BUDDY::INFO("追加 FreeBlock 池: [%p, %p)", pool_paddr.addr(),
                         (pool_paddr + FREEBLOCK_EXPAND_PAGES * PAGESIZE).addr());
    void_return();
}

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

    auto expand_res = add_new_pool();
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
    init_pool0();
}

void BuddyAllocator::add_memory_range(PhyAddr paddr, size_t pages) {
    size_t remain = pages;
    PhyAddr addr  = paddr;

    while (remain > 0) {
        size_t order = 0;
        while (order < MAX_BUDDY_ORDER) {
            size_t try_pages = 1UL << (order + 1);
            size_t try_size  = try_pages << 12;
            if (try_pages <= remain && addr.aligned(try_size)) {
                ++order;
            } else {
                break;
            }
        }

        put_page_in_order(addr, static_cast<int>(order));

        size_t block_pages = 1ul << order;
        addr              += block_pages << 12;
        remain            -= block_pages;
    }
}

Result<PhyAddr> BuddyAllocator::get_free_page(size_t frame_count) {
    if (frame_count == 0) {
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    if (frame_count > (1ul << MAX_BUDDY_ORDER)) {
        loggers::BUDDY::ERROR("请求的页数 %u 超出最大支持的范围",
                              static_cast<unsigned>(frame_count));
        unexpect_return(ErrCode::INVALID_PARAM);
    }

    auto fetch_res = fetch_frame_order(pages2order(frame_count));
    if (!fetch_res.has_value()) {
        unexpect_return(fetch_res.error());
    }

    PhyAddr paddr = fetch_res.value();
    size_t order  = static_cast<size_t>(pages2order(frame_count));
    size_t pages  = 1ul << order;
    if (pages > frame_count) {
        add_memory_range(paddr + frame_count * PAGESIZE, pages - frame_count);
    }

    loggers::BUDDY::DEBUG("分配了 %u 页物理内存: [%p, %p)",
                          static_cast<unsigned>(frame_count), paddr.addr(),
                          (paddr + frame_count * PAGESIZE).addr());

    auto expand_res = maybe_expand_pool();
    if (!expand_res.has_value()) {
        unexpect_return(expand_res.error());
    }
    return paddr;
}

Result<PhyAddr> BuddyAllocator::get_free_pages_in_order(size_t order) {
    if (order > MAX_BUDDY_ORDER) {
        loggers::BUDDY::ERROR("无可用内存块: order %u 超出范围",
                              static_cast<unsigned>(order));
        unexpect_return(ErrCode::INVALID_PARAM);
    }
    return fetch_frame_order(order);
}

void BuddyAllocator::put_page(PhyAddr paddr, size_t frame_count) {
    if (!paddr.nonnull() || frame_count == 0) {
        return;
    }

    assert(paddr.aligned<PAGESIZE>());
    add_memory_range(paddr, frame_count);
}

void BuddyAllocator::put_page_in_order(PhyAddr paddr, int order) {
    if (!paddr.nonnull()) {
        return;
    }

    assert(order >= 0);
    assert(order <= MAX_BUDDY_ORDER);
    assert(paddr.aligned(block_size_for_order(static_cast<size_t>(order))));

    PhyAddr current_paddr = paddr;
    int current_order     = order;

    while (current_order <= MAX_BUDDY_ORDER) {
        auto node_res = alloc_freeblock();
        assert(node_res.has_value());
        FreeBlock *node = node_res.value();
        node->paddr     = current_paddr;
        node->order     = static_cast<size_t>(current_order);
        node->prev      = nullptr;
        node->next      = nullptr;
        link_block(node);

        if (current_order == MAX_BUDDY_ORDER) {
            break;
        }

        FreeBlock *buddy = find_buddy_node(node);
        if (buddy == nullptr) {
            break;
        }

        size_t size          = block_size_for_order(static_cast<size_t>(current_order));
        PhyAddr buddy_paddr  = buddy->paddr;
        PhyAddr merged_paddr =
            buddy_paddr < current_paddr ? buddy_paddr : current_paddr;

        loggers::BUDDY::DEBUG("将 [%p, %p) 与 [%p, %p) 合并为 [%p, %p)",
                              current_paddr.addr(), (current_paddr + size).addr(),
                              buddy_paddr.addr(), (buddy_paddr + size).addr(),
                              merged_paddr.addr(),
                              (merged_paddr + size * 2).addr());

        unlink_block(node);
        unlink_block(buddy);
        free_freeblock(node);
        free_freeblock(buddy);

        current_paddr = merged_paddr;
        ++current_order;
    }
}

Result<PhyAddr> BuddyAllocator::fetch_frame_order(size_t order) {
    size_t current_order = order;
    while (current_order <= MAX_BUDDY_ORDER) {
        if (free_area[current_order] != nullptr) {
            break;
        }
        ++current_order;
    }

    if (current_order > MAX_BUDDY_ORDER) {
        loggers::BUDDY::ERROR("无可用内存块");
        unexpect_return(ErrCode::OUT_OF_MEMORY);
    }

    FreeBlock *node = free_area[current_order];
    unlink_block(node);
    PhyAddr paddr = node->paddr;
    free_freeblock(node);

    while (current_order > order) {
        --current_order;
        size_t size         = block_size_for_order(current_order);
        PhyAddr buddy_paddr = paddr + size;
        loggers::BUDDY::DEBUG("将 [%p, %p) 分割为 [%p, %p) 和 [%p, %p)",
                              paddr.addr(), (paddr + (size << 1)).addr(),
                              paddr.addr(), (paddr + size).addr(),
                              buddy_paddr.addr(), (buddy_paddr + size).addr());
        put_page_in_order(buddy_paddr, static_cast<int>(current_order));
    }
    return paddr;
}
