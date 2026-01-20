/**
 * @file trait.h
 * @author theflysong (song_of_the_fly@163.com)
 * @brief Riscv64架构Trait
 * @version alpha-1.0.0
 * @date 2026-01-19
 *
 * @copyright Copyright (c) 2026
 *
 */

#pragma once

#include <arch/trait.h>

class Riscv64Serial {
public:
    static void serial_write_char(char ch);
    static void serial_write_string(const char *str);
};

static_assert(ArchSerialTrait<Riscv64Serial>);
typedef Riscv64Serial ArchSerial;

class Riscv64Initialization {
public:
    static void pre_init(void);
    static void post_init(void);
};

static_assert(ArchInitializationTrait<Riscv64Initialization>);
typedef Riscv64Initialization ArchInitialization;

class Riscv64MemoryLayout {
public:
    /**
     * @brief 检测内存布局
     *
     * @param regions 内存区域数组
     * @param cnt 数组大小
     * @return 内存区域数量, < 0说明数组大小不足
     */
    static int detect_memory_layout(MemRegion *regions, int cnt);
};

static_assert(ArchMemLayoutTrait<Riscv64MemoryLayout>);
typedef Riscv64MemoryLayout ArchMemoryLayout;