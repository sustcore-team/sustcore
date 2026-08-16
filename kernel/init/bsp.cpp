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
#include <arch/timer.h>
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
#include <timer/deadline.h>
#include <timer/timer_engine.h>
#ifdef CONFIG_KERNEL_SELFTEST
#include <test/framework.h>
#endif

#include <utility>

namespace kernel::init::detail {
    void handle_clock_interrupt(void *, const device::interrupt::Event &) noexcept {
        auto &deadlines = kernel::timer::bsp_deadline_state();
        auto interrupt  = deadlines.begin_interrupt(hal::CpuClock::instance().current_time());

        // engine 与 scheduler 都会向 DeadlineState 发布下一绝对值；这里不持有 coordinator 锁。
        kernel::timer::bsp_timer_engine().progress(interrupt.now());
        if (interrupt.preemption_due())
            scheduler::instance().request_preemption();
        deadlines.end_interrupt(std::move(interrupt));
    }
}  // namespace kernel::init::detail

extern "C" [[noreturn]] void bsp_main() {
    hal::install_runtime_exception_vectors();

    auto device_result = ::device::initialize();
    if (!device_result)
        kernel::log::panic("设备目录初始化失败: {}", device_result.error());
    const auto &devices = ::device::catalog();
    kernel::log::info("设备目录已发布: devices={}, cpus={}, controllers={}, timebase={}Hz",
                      devices.device_count(), devices.cpu_count(), devices.controller_count(),
                      devices.platform().timebase_frequency_hz);
#if defined(__ARCH_RISCV64__)
    auto plic_result = riscv64::device::interrupt::Plic::initialize_from_catalog();
    if (!plic_result)
        kernel::log::panic("PLIC IRQ domain 初始化失败: {}", tay::to_string(plic_result.error()));
#endif
    init::advance(init::Milestone::VIRTUAL_MEMORY_READY, init::Milestone::FIRMWARE_READY);
    init::advance(init::Milestone::FIRMWARE_READY, init::Milestone::CPU_TOPOLOGY_READY);
    init::advance(init::Milestone::CPU_TOPOLOGY_READY, init::Milestone::TRAPS_READY);
    init::advance(init::Milestone::TRAPS_READY, init::Milestone::IRQ_READY);

    auto &clock = hal::CpuClock::instance();
    clock.initialize(devices.platform().timebase_frequency_hz);
    kernel::timer::bsp_deadline_state().initialize(clock);
    kernel::timer::bsp_timer_engine().initialize(kernel::timer::bsp_deadline_state());
    auto timer_subscription = device::interrupt::subscribe(
        device::interrupt::TIMER_LINE, kernel::init::detail::handle_clock_interrupt);
    if (!timer_subscription)
        kernel::log::panic("CPU timer 中断订阅失败: {}",
                           tay::to_string(timer_subscription.error()));
    init::advance(init::Milestone::IRQ_READY, init::Milestone::TIMER_READY);

#ifdef CONFIG_KERNEL_SELFTEST
    kernel::test::run_phase(kernel::test::Phase::POST_TIMER_INITIALIZATION);
#endif

    const size_t init_reclaimed = memory::reclaim_init_memory();
    kernel::log::info("已释放 init 内存: {} 页", init_reclaimed);
    kernel::log::info("KernelMM、KernelSpace 与全局内核堆已就绪");
    init::run_kinit();
}
