/**
 * @file int.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 中断系统兼容转发头
 * @version alpha-1.0.0
 * @date 2026-05-31
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <driver/int/base.h>
#if defined(__ARCH_riscv64__)
#include <arch/riscv64/device/clock.h>
#elif defined(__ARCH_loongarch64__)
#include <arch/loongarch64/device/clock.h>
#endif

namespace device {
    using driver::DriverBase;
    using driver::ClockSource;
    using driver::Alarm;
    using driver::ClockEvent;
    using driver::ClockEventInfo;
    using driver::ExpireAction;
    using driver::ExpireActionEntry;
    using driver::ExpireActionQueue;
    using driver::TimeKeeper;
    using driver::IrqTrigger;
    using driver::IrqResolveResult;
    using driver::IrqEvent;
    using driver::IrqDomain;
    using driver::IrqChip;
    template <size_t MAX_HW_IRQ>
    using LinearIrqDomain = driver::LinearIrqDomain<MAX_HW_IRQ>;
    using driver::IrqManager;
#if defined(__ARCH_riscv64__)
    using ClintAlarm = riscv::ClintAlarm;
#elif defined(__ARCH_loongarch64__)
    using CSRTimer = la64::CSRTimer;
#endif
}  // namespace device
