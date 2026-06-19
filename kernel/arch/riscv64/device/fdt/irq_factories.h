/**
 * @file irq_factories.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V FDT IRQ 工厂注册
 * @version alpha-1.0.0
 * @date 2026-06-20
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <device/fdt/provider.h>

namespace riscv::fdt {
    void register_irq_factories(const ::fdt::FDTProvider &provider) noexcept;
}
