/**
 * @file cpu.cpp
 * @brief CPU 拓扑和固定生命周期状态 selftest。
 */

#include <cpu/topology.h>
#include <device/catalog.h>
#include <test/cases.h>

namespace kernel::test::cases {
    void run_cpu_topology(Context &) noexcept {
        const auto &catalog = device::catalog();
        auto &topology      = cpu::topology();
        const auto snapshot = topology.snapshot();

        require(topology.ready(), "CPU topology must be published before selftests");
        require(snapshot.possible.count() == catalog.cpu_count(),
                "possible CPU set must match the immutable catalog");
        require(snapshot.started.count() == 1 && snapshot.online.count() == 1,
                "phase 2 must keep only the BSP started and online");
        require(snapshot.online.test(cpu::CpuId{0}), "BSP must be online as logical CPU zero");
        size_t iterated = 0;
        snapshot.possible.for_each([&](cpu::CpuId id) noexcept {
            ++iterated;
            require(id.value < snapshot.possible.count(),
                    "CPU set iteration must expose dense logical IDs");
        });
        require(iterated == snapshot.possible.count(), "CPU set iteration must cover all members");
        require(topology.state(cpu::CpuId{0}) == cpu::CpuState::ONLINE,
                "BSP lifecycle state must be ONLINE");
        require(topology.generation(cpu::CpuId{0}) >= 2,
                "BSP lifecycle generation must advance during publication");
        require(!topology.transition(cpu::CpuId{0}, cpu::CpuState::ONLINE, cpu::CpuState::ONLINE),
                "lifecycle transition must reject invalid state edges");

        for (const auto *descriptor = catalog.cpus_begin(); descriptor != catalog.cpus_end();
             ++descriptor)
        {
            const cpu::CpuId id{descriptor->cpu_id};
            require(topology.hw_id(id).value == descriptor->hw_id,
                    "logical CPU must retain its firmware hardware identifier");
            require(topology.logical_id(cpu::CpuHwId{descriptor->hw_id}) == id,
                    "hardware identifier lookup must round-trip through logical topology");
        }
    }
}  // namespace kernel::test::cases
