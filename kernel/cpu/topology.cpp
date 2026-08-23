/**
 * @file topology.cpp
 * @brief 固件 CPU 拓扑规范化和固定生命周期状态。
 */

#include <cpu/topology.h>
#include <device/catalog.h>
#include <tay/lock.h>

namespace cpu {
    namespace {
        constinit Topology topology_instance;

        [[nodiscard]] bool valid_transition(CpuState expected, CpuState next) noexcept {
            return (expected == CpuState::OFFLINE && next == CpuState::POSSIBLE) ||
                   (expected == CpuState::POSSIBLE && next == CpuState::STARTED) ||
                   (expected == CpuState::STARTED && next == CpuState::READY) ||
                   (expected == CpuState::READY && next == CpuState::ONLINE) ||
                   ((expected == CpuState::POSSIBLE || expected == CpuState::STARTED ||
                     expected == CpuState::READY) &&
                    next == CpuState::FAILED);
        }
    }  // namespace

    bool CpuSet::set(CpuId id) noexcept {
        return id.valid() && bits_.set(id.value).has_value();
    }

    bool CpuSet::reset(CpuId id) noexcept {
        return id.valid() && bits_.reset(id.value).has_value();
    }

    bool CpuSet::test(CpuId id) const noexcept {
        return id.valid() && bits_[id.value];
    }

    CpuSet CpuSet::without(CpuId id) const noexcept {
        CpuSet result = *this;
        static_cast<void>(result.reset(id));
        return result;
    }

    tay::expected<void, tay::error_code> Topology::initialize(
        const device::Catalog &catalog) noexcept {
        if (published_.load(std::memory_order_acquire))
            return {};
        if (!catalog.ready() || catalog.cpu_count() == 0 || catalog.cpu_count() > MAX_CPUS)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        const auto *bsp = catalog.bsp();
        if (bsp == nullptr || bsp->cpu_id != 0)
            return tay::Err(tay::error_code::INVALID_ARGUMENT);

        CpuSet possible;
        CpuHwId hardware_ids[MAX_CPUS]{};
        size_t descriptor_count = 0;
        for (const auto *descriptor = catalog.cpus_begin(); descriptor != catalog.cpus_end();
             ++descriptor, ++descriptor_count)
        {
            const CpuId id{descriptor->cpu_id};
            if (!descriptor->enabled || !id.valid() || id.value >= catalog.cpu_count() ||
                possible.test(id))
                return tay::Err(tay::error_code::INVALID_ARGUMENT);
            for (size_t index = 0; index < MAX_CPUS; ++index) {
                const CpuId existing_id{static_cast<u32_t>(index)};
                if (possible.test(existing_id) && hardware_ids[index].value == descriptor->hw_id)
                    return tay::Err(tay::error_code::INVALID_ARGUMENT);
            }
            hardware_ids[id.value] = CpuHwId{descriptor->hw_id};
            static_cast<void>(possible.set(id));
        }
        if (descriptor_count != catalog.cpu_count())
            return tay::Err(tay::error_code::INVALID_ARGUMENT);
        for (size_t index = 0; index < descriptor_count; ++index)
            if (!possible.test(CpuId{static_cast<u32_t>(index)}))
                return tay::Err(tay::error_code::INVALID_ARGUMENT);

        possible_ = possible;
        for (size_t index = 0; index < descriptor_count; ++index) {
            const CpuId id{static_cast<u32_t>(index)};
            hardware_ids_[index] = hardware_ids[index];
            auto *storage        = try_slot(id);
            storage->local.id    = id;
            storage->local.hw_id = hardware_ids[index];
            storage->lifecycle.store(encode_lifecycle(CpuState::POSSIBLE, 1),
                                     std::memory_order_release);
        }

        const CpuId bsp_id{0};
        started_bits_.store(u64_t{1}, std::memory_order_release);
        online_bits_.store(u64_t{1}, std::memory_order_release);
        auto *bsp_state = try_slot(bsp_id);
        bsp_state->lifecycle.store(encode_lifecycle(CpuState::ONLINE, 2),
                                   std::memory_order_release);
        published_.store(true, std::memory_order_release);
        return {};
    }

    bool Topology::ready() const noexcept {
        return published_.load(std::memory_order_acquire);
    }

    CpuSnapshot Topology::snapshot() const noexcept {
        if (!ready())
            return {};
        while (true) {
            const u64_t version = snapshot_version_.load(std::memory_order_acquire);
            if ((version & 1) != 0)
                continue;
            CpuSnapshot result{.possible = possible_};
            const u64_t started = started_bits_.load(std::memory_order_acquire);
            const u64_t online  = online_bits_.load(std::memory_order_acquire);
            for (size_t index = 0; index < MAX_CPUS; ++index) {
                const CpuId id{static_cast<u32_t>(index)};
                if ((started & (u64_t{1} << index)) != 0)
                    static_cast<void>(result.started.set(id));
                if ((online & (u64_t{1} << index)) != 0)
                    static_cast<void>(result.online.set(id));
            }
            if (snapshot_version_.load(std::memory_order_acquire) == version)
                return result;
        }
    }

    CpuHwId Topology::hw_id(CpuId id) const noexcept {
        return ready() && id.valid() ? hardware_ids_[id.value] : CpuHwId{};
    }

    CpuId Topology::logical_id(CpuHwId hardware_id_value) const noexcept {
        if (!ready())
            return CpuId{};
        for (size_t index = 0; index < MAX_CPUS; ++index) {
            const CpuId id{static_cast<u32_t>(index)};
            if (possible_.test(id) && hardware_ids_[index] == hardware_id_value)
                return id;
        }
        return CpuId{};
    }

    CpuState Topology::state(CpuId id) const noexcept {
        const auto *storage = try_slot(id);
        return storage == nullptr
                   ? CpuState::OFFLINE
                   : lifecycle_state(storage->lifecycle.load(std::memory_order_acquire));
    }

    u32_t Topology::generation(CpuId id) const noexcept {
        const auto *storage = try_slot(id);
        return storage == nullptr
                   ? 0
                   : lifecycle_gen(storage->lifecycle.load(std::memory_order_acquire));
    }

    bool Topology::transition(CpuId id, CpuState expected, CpuState next) noexcept {
        if (!ready())
            return false;
        tay::unique_lock transition{transition_lock_, tay::try_to_lock};
        if (!transition)
            return false;
        auto *storage = try_slot(id);
        if (storage == nullptr)
            return false;
        if (!possible_.test(id) || !valid_transition(expected, next))
            return false;
        u64_t observed = storage->lifecycle.load(std::memory_order_acquire);
        if (lifecycle_state(observed) != expected)
            return false;
        const u32_t next_generation = lifecycle_gen(observed) + 1;
        snapshot_version_.fetch_add(1, std::memory_order_acq_rel);
        const u64_t old_started = started_bits_.load(std::memory_order_relaxed);
        const u64_t old_online  = online_bits_.load(std::memory_order_relaxed);
        const u64_t mask        = u64_t{1} << id.value;
        u64_t next_started      = old_started;
        u64_t next_online       = old_online;
        if (next == CpuState::STARTED)
            next_started |= mask;
        else if (next == CpuState::ONLINE)
            next_online |= mask;
        else if (next == CpuState::FAILED) {
            next_started &= ~mask;
            next_online  &= ~mask;
        }
        started_bits_.store(next_started, std::memory_order_release);
        online_bits_.store(next_online, std::memory_order_release);
        const u64_t desired = encode_lifecycle(next, next_generation);
        if (!storage->lifecycle.compare_exchange_strong(
                observed, desired, std::memory_order_release, std::memory_order_acquire))
        {
            started_bits_.store(old_started, std::memory_order_release);
            online_bits_.store(old_online, std::memory_order_release);
            snapshot_version_.fetch_add(1, std::memory_order_release);
            return false;
        }
        snapshot_version_.fetch_add(1, std::memory_order_release);
        return true;
    }

    Topology &topology() noexcept {
        return topology_instance;
    }
}  // namespace cpu
