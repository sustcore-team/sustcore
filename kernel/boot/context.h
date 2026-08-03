/**
 * @file context.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 内核持久化启动上下文
 * @version 0.1.0-dev.1
 * @date 2026-08-02
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <boot/boot.h>

#include <cstddef>

namespace boot {
    /**
     * @brief 内核永久持有的 BootInfo/FDT 副本及其物理内存所有权状态。
     * @note Context 地址稳定；reclaim 后元数据仍可查询，但对应 boot 页已归还 Buddy。
     */
    struct Context {
        BootInfoHeader *info = nullptr;
        void *fdt            = nullptr;
        PhyAddr info_paddr{};
        PhyAddr fdt_paddr{};
        size_t info_sz    = 0;
        size_t fdt_sz     = 0;
        size_t info_pages = 0;
        size_t fdt_pages  = 0;
        bool reclaimed    = false;
    };

    /**
     * @brief 复制并持久化已校验的 BootInfo 与 FDT。
     * @note 仅允许 BSP 在 Buddy 初始化后、回收启动内存前调用一次。
     */
    void preserve(const BootInfoHeader &source) noexcept;

    /**
     * @brief 获取已初始化的永久 Boot Context。
     * @warning preserve() 之前调用会触发 panic。
     */
    Context &context() noexcept;

    /**
     * @brief 将 BOOT_RECLAIMABLE 页归还 Buddy，但保留当前执行所需的 init 区域。
     * @return 本次实际回收的物理页数。
     */
    size_t reclaim_boot_memory() noexcept;
}  // namespace boot
