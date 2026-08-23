/**
 * @file early.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief BSP 通用早期启动接口
 * @version 0.1.0-dev.1
 * @date 2026-08-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/boot.h>

#include <cstddef>

namespace boot {
    /**
     * @brief 获取已校验并由内核持有的 BootInfo 副本.
     *
     * 该接口仅在 `__bsp_early_main()` 完成后可用.
     */
    [[nodiscard]]
    const BootInfoHeader *early_bootinfo() noexcept;

    /**
     * @brief 获取 BSP 的硬件 ID.
     */
    [[nodiscard]]
    size_t early_boot_hwid() noexcept;

    /**
     * @brief 从 FREE parent 扣除 reservation 与 metadata 后发布全部可用区域。
     * @note Buddy 只接收最终可用区，不解释 BootInfo 或 PageDesc。
     */
    void publish_areas(const BootInfoHeader &bootinfo) noexcept;

}  // namespace boot

/**
 * @brief 临时页表已经启用后进入的统一 BSP 入口.
 *
 * 架构汇编入口保证中断关闭并切换到永久 BSP 栈后调用此函数.
 */
extern "C" [[noreturn]] void __bsp_early_main(size_t bsp_hwid,
                                              const BootInfoHeader *source_bootinfo);
