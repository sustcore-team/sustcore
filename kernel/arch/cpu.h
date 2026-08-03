/**
 * @file cpu.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 通用处理器基础操作
 * @version 0.1.0-dev.1
 * @date 2026-08-03
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/namespace.h>
#include <feature/attributes.h>

SUSTCORE_ARCH_NAMESPACE_BEGIN
namespace hal {
    /**
     * @brief 在自旋等待循环中向处理器提供低开销让步提示。
     * @note 不保证线程调度或内存顺序；同步仍必须由原子操作或锁提供。
     */
    __ATTR_ALWAYS_INLINE__ void cpu_relax() noexcept {
#if defined(__ARCH_RISCV64__)
        // Zihintpause encoding. Using the encoding keeps older assemblers from
        // rejecting the mnemonic when the configured ISA string omits it.
        asm volatile(".4byte 0x0100000f" ::: "memory");
#elif defined(__ARCH_LOONGARCH64__)
        asm volatile("nop" ::: "memory");
#else
#error "unsupported architecture"
#endif
    }
}  // namespace hal
SUSTCORE_ARCH_NAMESPACE_END
