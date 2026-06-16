/**
 * @file clint.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief RISC-V CLINT 兼容转发头
 * @version alpha-1.0.0
 * @date 2026-06-16
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/riscv64/clint.h>

namespace driver {
    using Clint = riscv::Clint;
}
