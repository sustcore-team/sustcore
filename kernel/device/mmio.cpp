/**
 * @file mmio.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备 MMIO capability 对象与内核映射实现
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#include <device/mmio.h>
#include <log.h>
#include <memory/virtual/kernel/kernel_space.h>

#include <new>

namespace device {
    tay::expected<cap::ObjectRef<MmioObject>, MmioError> MmioObject::create(PhyArea area) noexcept {
        if (area.nullable() || area.begin >= area.end)
            return tay::Err(MmioError::InvalidPhysicalArea(area));
        if (area.end.arith() > KPA_START - (PAGE_SIZE - 1))
            return tay::Err(MmioError::SizeOverflow(area.begin, area.size()));
        const auto aligned_begin = area.begin.page_align_down();
        const auto aligned_end   = area.end.page_align_up();
        if (aligned_begin >= aligned_end)
            return tay::Err(MmioError::SizeOverflow(area.begin, area.size()));
        const PhyArea aligned{aligned_begin, aligned_end};
        auto *object = new (std::nothrow) MmioObject(area, aligned);
        if (object == nullptr)
            return tay::Err(MmioError::OutOfMemory());
        return cap::ObjectRef<MmioObject>(*object);
    }

    MmioObject::~MmioObject() noexcept {
        if (mapped_) {
            auto unmapped = unmap_from_kernel();
            if (!unmapped)
                kernel::log::panic("销毁 MMIO 对象时无法解除内核映射: {}", unmapped.error());
        }
    }

    tay::expected<KvaAddr, MmioError> MmioObject::map_to_kernel() noexcept {
        if (mapped_)
            return tay::Err(MmioError::MappingConflict(area_));
        auto *space = memory::try_kernel_space();
        if (space == nullptr)
            return tay::Err(MmioError::KernelSpaceUnavailable());
        auto mapped = space->map_device(aligned_area_.begin, aligned_area_.size(),
                                        memory::PageFlags{.readable   = true,
                                                          .writable   = true,
                                                          .executable = false,
                                                          .cache      = memory::CacheMode::DEVICE});
        if (!mapped)
            return tay::Err(MmioError::PagingFailed(std::move(mapped.error())));
        mapped_base_ = *mapped;
        mapped_      = true;
        return kernel_base();
    }

    tay::expected<void, MmioError> MmioObject::unmap_from_kernel() noexcept {
        if (!mapped_)
            return tay::Err(MmioError::NotMapped());
        auto *space = memory::try_kernel_space();
        if (space == nullptr)
            return tay::Err(MmioError::KernelSpaceUnavailable());
        auto result = space->unmap_device(aligned_area_.begin, aligned_area_.size());
        if (!result)
            return tay::Err(MmioError::PagingFailed(std::move(result.error())));
        mapped_base_ = KvaAddr::null;
        mapped_      = false;
        return {};
    }
}  // namespace device
