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

#include <arch/riscv64/csr.h>
#include <arch/riscv64/ctx.h>
#include <arch/riscv64/ctxlayout.h>
#include <arch/trait.h>
#include <task/startup.h>

namespace rv64 {
    class EarlySerial {
    public:
        static void serial_write_char(char ch);
        static void serial_write_string(size_t len, const char *str);
    };

    static_assert(EarlySerialTrait<EarlySerial>);

    class Initialization {
    public:
        static void pre_init(void);
        static void post_init(void);
        static Result<void> init_clock();
    };

    static_assert(InitializationTrait<Initialization>);

    struct Interrupt {
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

        static bool enabled() {
            csr_sstatus_t sstatus = csr_get_sstatus();
            return sstatus.sie;
        }
    };

    static_assert(InterruptTrait<Interrupt>);

    struct Idle {
        static void idle();
    };
    static_assert(IdleTrait<Idle>);
}  // namespace rv64

#include <arch/riscv64/mem/pageman.h>
