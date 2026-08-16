/**
 * @file mmio.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备 MMIO capability 对象与内核映射接口
 * @version 0.1.0-dev.1
 * @date 2026-08-14
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/mmio_error.h>
#include <obj/kernel_object.h>
#include <sustcore/addr.h>
#include <tay/expected.h>

#include <cstddef>

namespace device {
    class MmioObject final : public cap::TypedKernelObject<MmioObject, cap::ObjectType::MMIO> {
    public:
        static constexpr cap::ObjectType TYPE = cap::ObjectType::MMIO;

        [[nodiscard]] static tay::expected<cap::ObjectRef<MmioObject>, MmioError> create(
            PhyArea area) noexcept;

        MmioObject(const MmioObject &)            = delete;
        MmioObject &operator=(const MmioObject &) = delete;
        MmioObject(MmioObject &&)                 = delete;
        MmioObject &operator=(MmioObject &&)      = delete;
        ~MmioObject() noexcept;

        [[nodiscard]] PhyArea area() const noexcept {
            return area_;
        }

        [[nodiscard]] bool mapped() const noexcept {
            return mapped_;
        }

        /** @brief 仅内核驱动可调用；将对象的完整页区间映射到内核高半区。 */
        [[nodiscard]] tay::expected<KvaAddr, MmioError> map_to_kernel() noexcept;
        /** @brief 仅内核驱动可调用；解除该对象的内核映射。 */
        [[nodiscard]] tay::expected<void, MmioError> unmap_from_kernel() noexcept;

        [[nodiscard]] KvaAddr kernel_base() const noexcept {
            return mapped_ ? KvaAddr(mapped_base_.arith() + area_.begin.arith() -
                                     aligned_area_.begin.arith())
                           : KvaAddr::null;
        }

    private:
        explicit MmioObject(PhyArea area, PhyArea aligned_area) noexcept
            : area_(area), aligned_area_(aligned_area) {}

        PhyArea area_{};
        PhyArea aligned_area_{};
        KvaAddr mapped_base_{};
        bool mapped_ = false;
    };
}  // namespace device
