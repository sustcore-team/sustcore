/**
 * @file malloc.cpp
 * @author theflysong
 * @brief linux subsystem libc free-list heap allocator
 * @version alpha-1.0.0
 * @date 2026-06-25
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <prm.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
    constexpr size_t MALLOC_ALIGNMENT = 16;
    constexpr size_t MIN_SPLIT_SIZE   = MALLOC_ALIGNMENT;

    struct alignas(MALLOC_ALIGNMENT) BlockHeader {
        size_t size;
        bool free;
        BlockHeader *phys_prev;
        BlockHeader *phys_next;
        BlockHeader *free_prev;
        BlockHeader *free_next;
    };

    static_assert(sizeof(BlockHeader) % MALLOC_ALIGNMENT == 0);

    BlockHeader *_heap_tail = nullptr;
    BlockHeader *_free_head = nullptr;

    BlockHeader *absorb_next(BlockHeader *block);

    [[nodiscard]]
    constexpr size_t align_up(size_t value, size_t alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    [[nodiscard]]
    bool checked_payload_size(size_t requested, size_t &payload_size) noexcept {
        size_t normalized = requested == 0 ? 1 : requested;
        if (normalized > SIZE_MAX - MALLOC_ALIGNMENT + 1) {
            return false;
        }
        payload_size = align_up(normalized, MALLOC_ALIGNMENT);
        return payload_size <= static_cast<size_t>(PTRDIFF_MAX);
    }

    [[nodiscard]]
    bool checked_total_size(size_t payload_size, size_t &total_size) noexcept {
        if (SIZE_MAX - sizeof(BlockHeader) < payload_size) {
            return false;
        }
        total_size = sizeof(BlockHeader) + payload_size;
        return total_size <= static_cast<size_t>(PTRDIFF_MAX);
    }

    [[nodiscard]]
    void *payload(BlockHeader *block) noexcept {
        return reinterpret_cast<void *>(block + 1);
    }

    [[nodiscard]]
    BlockHeader *header_from_payload(void *ptr) noexcept {
        return static_cast<BlockHeader *>(ptr) - 1;
    }

    void remove_free(BlockHeader *block) noexcept {
        if (block->free_prev != nullptr) {
            block->free_prev->free_next = block->free_next;
        } else {
            _free_head = block->free_next;
        }

        if (block->free_next != nullptr) {
            block->free_next->free_prev = block->free_prev;
        }

        block->free_prev = nullptr;
        block->free_next = nullptr;
    }

    void insert_free(BlockHeader *block) noexcept {
        block->free      = true;
        block->free_prev = nullptr;
        block->free_next = _free_head;
        if (_free_head != nullptr) {
            _free_head->free_prev = block;
        }
        _free_head = block;
    }

    [[nodiscard]]
    BlockHeader *find_free(size_t payload_size) noexcept {
        for (BlockHeader *block = _free_head; block != nullptr;
             block              = block->free_next)
        {
            if (block->size >= payload_size) {
                return block;
            }
        }
        return nullptr;
    }

    void split_block(BlockHeader *block, size_t payload_size) noexcept {
        if (block->size < payload_size + sizeof(BlockHeader) + MIN_SPLIT_SIZE) {
            return;
        }

        auto *remain = reinterpret_cast<BlockHeader *>(
            reinterpret_cast<char *>(payload(block)) + payload_size);
        remain->size      = block->size - payload_size - sizeof(BlockHeader);
        remain->free      = true;
        remain->phys_prev = block;
        remain->phys_next = block->phys_next;
        remain->free_prev = nullptr;
        remain->free_next = nullptr;

        if (remain->phys_next != nullptr) {
            remain->phys_next->phys_prev = remain;
        } else {
            _heap_tail = remain;
        }

        block->size      = payload_size;
        block->phys_next = remain;

        if (remain->phys_next != nullptr && remain->phys_next->free) {
            remove_free(remain->phys_next);
            absorb_next(remain);
        }
        insert_free(remain);
    }

    [[nodiscard]]
    BlockHeader *request_from_heap(size_t payload_size) noexcept {
        size_t total_size = 0;
        if (!checked_total_size(payload_size, total_size)) {
            return nullptr;
        }

        size_t old_brk = __linuxss_ss_brk;
        if (old_brk > SIZE_MAX - total_size) {
            return nullptr;
        }

        size_t new_brk = old_brk + total_size;
        if (linuxss_brk(new_brk) != new_brk) {
            return nullptr;
        }

        auto *block = reinterpret_cast<BlockHeader *>(old_brk);
        block->size      = payload_size;
        block->free      = false;
        block->phys_prev = _heap_tail;
        block->phys_next = nullptr;
        block->free_prev = nullptr;
        block->free_next = nullptr;

        if (_heap_tail != nullptr) {
            _heap_tail->phys_next = block;
        }
        _heap_tail = block;
        return block;
    }

    [[nodiscard]]
    BlockHeader *absorb_next(BlockHeader *block) noexcept {
        BlockHeader *next = block->phys_next;
        block->size += sizeof(BlockHeader) + next->size;
        block->phys_next = next->phys_next;

        if (block->phys_next != nullptr) {
            block->phys_next->phys_prev = block;
        } else {
            _heap_tail = block;
        }

        return block;
    }

    [[nodiscard]]
    BlockHeader *coalesce(BlockHeader *block) noexcept {
        if (block->phys_prev != nullptr && block->phys_prev->free) {
            BlockHeader *prev = block->phys_prev;
            remove_free(prev);
            block = absorb_next(prev);
        }

        if (block->phys_next != nullptr && block->phys_next->free) {
            remove_free(block->phys_next);
            block = absorb_next(block);
        }

        return block;
    }

    [[nodiscard]]
    void *alloc(size_t requested_size) noexcept {
        size_t payload_size = 0;
        if (!checked_payload_size(requested_size, payload_size)) {
            return nullptr;
        }

        BlockHeader *block = find_free(payload_size);
        if (block != nullptr) {
            remove_free(block);
            block->free = false;
            split_block(block, payload_size);
            return payload(block);
        }

        block = request_from_heap(payload_size);
        if (block == nullptr) {
            return nullptr;
        }
        return payload(block);
    }
}  // namespace

extern "C" {
void *malloc(size_t size) {
    return alloc(size);
}

void free(void *ptr) {
    if (ptr == nullptr) {
        return;
    }

    BlockHeader *block = header_from_payload(ptr);
    block->free        = true;
    block->free_prev   = nullptr;
    block->free_next   = nullptr;
    insert_free(coalesce(block));
}

void *calloc(size_t nmemb, size_t size) {
    if (size != 0 && nmemb > SIZE_MAX / size) {
        return nullptr;
    }

    size_t total = nmemb * size;
    void *ptr    = malloc(total);
    if (ptr != nullptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (ptr == nullptr) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return nullptr;
    }

    size_t payload_size = 0;
    if (!checked_payload_size(size, payload_size)) {
        return nullptr;
    }

    BlockHeader *block = header_from_payload(ptr);
    if (block->size >= payload_size) {
        split_block(block, payload_size);
        return ptr;
    }

    if (block->phys_next != nullptr && block->phys_next->free &&
        block->size + sizeof(BlockHeader) + block->phys_next->size >=
            payload_size)
    {
        remove_free(block->phys_next);
        block = absorb_next(block);
        split_block(block, payload_size);
        return ptr;
    }

    void *new_ptr = malloc(size);
    if (new_ptr == nullptr) {
        return nullptr;
    }

    memcpy(new_ptr, ptr, block->size < size ? block->size : size);
    free(ptr);
    return new_ptr;
}
}
