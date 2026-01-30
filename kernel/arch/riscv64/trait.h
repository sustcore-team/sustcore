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
#include <arch/riscv64/csr.h>
#include <sus/types.h>

// 该功能必须通过宏来实现, 以保证其一定会内联到原代码片段中
#define RELOAD_SP()            \
    asm volatile(                  \
        "la sp, boot_stack_top\n"     \
    );

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

struct Riscv64Context {
    umb_t regs[31];
    umb_t sepc;
    csr_sstatus_t sstatus;

    static umb_t &pc(Riscv64Context *ctx) {
        return ctx->sepc;
    }

    static umb_t &sp(Riscv64Context *ctx) {
        return ctx->regs[2 - 1];  // x2 = sp
    }
};

static_assert(ArchContextTrait<Riscv64Context>);

struct Riscv64Interrupt {
    /**
    * @brief 初始化IVT
    *
    */
    static void init(void);

    /**
    * @brief 启用中断
    *
    */
    static void sti(void);

    /**
    * @brief 关闭中断
    *
    */
    static void cli(void);
};

static_assert(ArchInterruptTrait<Riscv64Interrupt>);

struct Riscv64WPFault {
    int reserved;
};
static_assert(ArchWPFaultTrait<Riscv64WPFault>);

#include <arch/riscv64/mem/sv39.h>