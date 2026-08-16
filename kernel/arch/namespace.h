/**
 * @file namespace.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 目标架构命名空间入口
 * @version 0.1.0-dev.1
 * @date 2026-08-04
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#if defined(__ARCH_RISCV64__)
#define SUSTCORE_ARCH_NAMESPACE riscv64
#elif defined(__ARCH_LOONGARCH64__)
#define SUSTCORE_ARCH_NAMESPACE loongarch64
#else
#error unsupported architecture
#endif

namespace SUSTCORE_ARCH_NAMESPACE {
    namespace hal {}
}  // namespace SUSTCORE_ARCH_NAMESPACE

// 架构无关代码只通过 hal 访问当前 ISA 的实现，不能导入整个架构命名空间。
namespace hal = SUSTCORE_ARCH_NAMESPACE::hal;

#define SUSTCORE_ARCH_NAMESPACE_BEGIN namespace SUSTCORE_ARCH_NAMESPACE {
#define SUSTCORE_ARCH_NAMESPACE_END   }
