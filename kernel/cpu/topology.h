/**
 * @file topology.h
 * @brief 固件 CPU 目录到固定逻辑 CPU 集合的不可变发布。
 */

#pragma once

#include <cpu/local.h>
#include <tay/bitmap.h>
#include <tay/expected.h>
#include <tay/format.h>
#include <tay/spinlock.h>

#include <atomic>
#include <cstddef>
#include <utility>

namespace device {
    class Catalog;
}

namespace cpu {
    using CpuSetStorage = tay::static_bitmap<MAX_CPUS>;

    class CpuSet final {
    public:
        constexpr CpuSet() noexcept = default;

        [[nodiscard]] bool set(CpuId id) noexcept;
        [[nodiscard]] bool reset(CpuId id) noexcept;
        [[nodiscard]] bool test(CpuId id) const noexcept;
        [[nodiscard]] size_t count() const noexcept {
            return bits_.count();
        }
        [[nodiscard]] bool empty() const noexcept {
            return bits_.none();
        }
        [[nodiscard]] CpuSet without(CpuId id) const noexcept;

        template <typename Function>
        void for_each(Function &&function) const noexcept {
            for (size_t index = 0; index < MAX_CPUS; ++index)
                if (bits_[index])
                    std::forward<Function>(function)(CpuId{static_cast<u32_t>(index)});
        }

        [[nodiscard]] CpuId first() const noexcept {
            const size_t index = bits_.find_first_set();
            return index == CpuSetStorage::npos ? CpuId{} : CpuId{static_cast<u32_t>(index)};
        }

    private:
        CpuSetStorage bits_{};

        friend class Topology;
    };

    struct CpuSnapshot final {
        CpuSet possible{};
        CpuSet started{};
        CpuSet online{};
    };

    class Topology final {
    public:
        [[nodiscard]] tay::expected<void, tay::error_code> initialize(
            const device::Catalog &catalog) noexcept;

        [[nodiscard]] bool ready() const noexcept;
        [[nodiscard]] CpuSnapshot snapshot() const noexcept;
        [[nodiscard]] CpuHwId hw_id(CpuId id) const noexcept;
        [[nodiscard]] CpuId logical_id(CpuHwId hw_id) const noexcept;
        [[nodiscard]] CpuId bsp() const noexcept {
            return CpuId{0};
        }

        [[nodiscard]] CpuState state(CpuId id) const noexcept;
        [[nodiscard]] u32_t generation(CpuId id) const noexcept;
        [[nodiscard]] bool transition(CpuId id, CpuState expected, CpuState next) noexcept;

    private:
        CpuSet possible_{};
        CpuHwId hardware_ids_[MAX_CPUS]{};
        std::atomic<u64_t> started_bits_{0};
        std::atomic<u64_t> online_bits_{0};
        std::atomic<u64_t> snapshot_version_{0};
        tay::spinlock transition_lock_{};
        std::atomic<bool> published_{false};
    };

    [[nodiscard]] Topology &topology() noexcept;
}  // namespace cpu

namespace tay {
    template <>
    struct formatter<cpu::CpuSet> : detail::empty_spec_formatter {
        template <class FormatContext>
        typename FormatContext::iterator format(const cpu::CpuSet &set,
                                                FormatContext &context) const {
            context.put('{');
            bool first = true;
            set.for_each([&](cpu::CpuId id) {
                if (!first)
                    context.write(", ");
                first = false;
                context.format("{}", id.value);
            });
            context.put('}');
            return context.out();
        }
    };
}  // namespace tay
