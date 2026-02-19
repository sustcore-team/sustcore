/**
 * @file slub.cpp
 * @author jeromeyao (yaoshengqi726@outlook.com)
 * theflysong(song_of_the_fly@163.com)
 * @brief SLUB Allocator
 * @version alpha-1.0.0
 * @date 2026-02-13
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <mem/slub.h>

namespace slub {
    util::Defer<SlubAllocator<MixedSizeAllocator::AllocRecord>> MixedSizeAllocator::AllocRecord::SLUB;
    util::Defer<util::IntrusiveList<MixedSizeAllocator::AllocRecord>> MixedSizeAllocator::alloc_records;
}  // namespace slub
