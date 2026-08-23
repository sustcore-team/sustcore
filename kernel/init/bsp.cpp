/**
 * @file bsp.cpp
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 设备目录、IRQ、CPU 时钟、timer 与 init 内存回收的 BSP bring-up。
 * @version 0.1.0-dev.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 */

#include <arch/interrupt.h>
#include <arch/smp.h>
#include <arch/timer.h>
#include <cpu/topology.h>
#if defined(__ARCH_RISCV64__)
#include <arch/riscv64/device/plic.h>
#endif
#include <device/catalog.h>
#include <device/interrupt.h>
#include <init/kinit.h>
#include <init/milestones.h>
#include <log.h>
#include <memory/reclaim.h>
#include <scheduler/scheduler.h>
#include <smp/shootdown.h>
#include <timer/deadline.h>
#include <timer/hrtimer.h>
#ifdef CONFIG_KERNEL_SELFTEST
#include <test/framework.h>
#endif

#include <utility>

namespace kernel::init::detail {
    void handle_timer_irq(void *, const device::interrupt::TrapEvent &) noexcept {
        const auto current = cpu::current_id();
        auto &deadlines    = kernel::timer::local_deadline_mux();
        auto interrupt     = deadlines.begin_interrupt(hal::Clock::instance().now());

        // precision timer engine 仍固定于 BSP；AP 的 timer IRQ 只消费本地 RR deadline，绝不
        // 推进 BSP 的 timer heap 或将本地事件回流到 CPU 0。
        if (current.value == 0)
            kernel::timer::bsp_hrtimers().progress(interrupt.now());
        if (interrupt.preemption_due())
            scheduler::local().request_preempt();
        deadlines.end_interrupt(std::move(interrupt));
    }
}  // namespace kernel::init::detail

extern "C" [[noreturn]] void bsp_main() {
    hal::set_trap_vectors();

    auto device_result = ::device::initialize();
    if (!device_result)
        kernel::log::panic("设备目录初始化失败: {}", device_result.error());
    const auto &devices  = ::device::catalog();
    auto topology_result = cpu::topology().initialize(devices);
    if (!topology_result)
        kernel::log::panic("CPU 拓扑初始化失败: {}", tay::to_string(topology_result.error()));
    const auto cpu_snapshot = cpu::topology().snapshot();
    kernel::log::info("设备目录已发布: devices={}, cpus={}, controllers={}, timebase={}Hz",
                      devices.device_count(), devices.cpu_count(), devices.irq_ctrl_count(),
                      devices.platform().timebase_hz);
    kernel::log::info("CPU 拓扑已发布: possible={}, started={}, online={}, cpu_set={}, bsp_hwid={}",
                      cpu_snapshot.possible.count(), cpu_snapshot.started.count(),
                      cpu_snapshot.online.count(), cpu_snapshot.online,
                      cpu::topology().hw_id(cpu::CpuId{0}).value);
#if defined(__ARCH_RISCV64__)
    auto plic_result = riscv64::device::interrupt::Plic::init_catalog();
    if (!plic_result)
        kernel::log::panic("PLIC IRQ domain 初始化失败: {}", tay::to_string(plic_result.error()));
#endif
    hal::init_ipi();
    smp::init_shootdown();
    init::advance(init::Milestone::VIRTUAL_MEMORY_READY, init::Milestone::FIRMWARE_READY);
    init::advance(init::Milestone::FIRMWARE_READY, init::Milestone::CPU_TOPOLOGY_READY);
    init::advance(init::Milestone::CPU_TOPOLOGY_READY, init::Milestone::TRAPS_READY);
    init::advance(init::Milestone::TRAPS_READY, init::Milestone::IRQ_READY);

    auto &clock = hal::Clock::instance();
    clock.init_freq(devices.platform().timebase_hz);
    clock.initialize_local();
    kernel::timer::bsp_deadline_mux().initialize(clock);
    kernel::timer::bsp_hrtimers().initialize(kernel::timer::bsp_deadline_mux());
    auto timer_subscription = device::interrupt::subscribe(device::interrupt::TIMER_LINE,
                                                           kernel::init::detail::handle_timer_irq);
    if (!timer_subscription)
        kernel::log::panic("CPU timer 中断订阅失败: {}",
                           tay::to_string(timer_subscription.error()));
    init::advance(init::Milestone::IRQ_READY, init::Milestone::TIMER_READY);

#ifdef CONFIG_KERNEL_SELFTEST
    kernel::test::run_phase(kernel::test::Phase::POST_TIMER_INIT);
#endif

    const size_t init_reclaimed = memory::reclaim_init();
    kernel::log::info("已释放 init 内存: {} 页", init_reclaimed);
    kernel::log::info("KernelMM、KernelVm 与全局内核堆已就绪");
    init::run_kinit();
}
