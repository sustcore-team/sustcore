/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief 架构 Trait 定义
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <concept>
#include <cstddef>

/**
 * @brief 架构串口 Trait
 *
 * @tparam T 架构串口类
 */
template <typename T>
concept ArchSerialTrait = requires(char ch, const char *str) {
    {
        T::serial_write_char(ch)
    } -> std::same_as<void>;
    {
        T::serial_write_string(str)
    } -> std::same_as<void>;
};

/**
 * @brief 内核启动函数
 *
 */
void kernel_setup(void);

/**
 * @brief 架构初始化 Trait
 *
 * @tparam T 架构初始化类
 */
template <typename T>
concept ArchInitializationTrait = requires() {
    {
        T::pre_init()
    } -> std::same_as<void>;
    {
        T::post_init()
    } -> std::same_as<void>;
};

/**
 * @brief 内存区域
 *
 */
struct MemRegion {
    /**
     * @brief 内存状态
     *
     */
    enum class MemoryStatus {
        FREE             = 0,
        RESERVED         = 1,
        ACPI_RECLAIMABLE = 2,
        ACPI_NVS         = 3,
        BAD_MEMORY       = 4
    };

    void *ptr;
    size_t size;
    MemoryStatus status;
};

/**
 * @brief 架构内存布局 Trait
 *
 * @tparam T 架构内存布局类
 */
template <typename T>
concept ArchMemLayoutTrait = requires(MemRegion *regions, int cnt) {
    {
        T::detect_memory_layout(regions, cnt)
    } -> std::same_as<int>;
};